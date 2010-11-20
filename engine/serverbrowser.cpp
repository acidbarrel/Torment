// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

#include "pch.h"
#include "engine.h"
#include "SDL_thread.h"

struct resolverthread
{
    SDL_Thread *thread;
    const char *query;
    int starttime;
};

struct resolverresult
{
    const char *query;
    ENetAddress address;
};

vector<resolverthread> resolverthreads;
vector<const char *> resolverqueries;
vector<resolverresult> resolverresults;
SDL_mutex *resolvermutex;
SDL_cond *querycond, *resultcond;

#define RESOLVERTHREADS 1
#define RESOLVERLIMIT 3000

int resolverloop(void * data)
{
    resolverthread *rt = (resolverthread *)data;
    SDL_LockMutex(resolvermutex);
    SDL_Thread *thread = rt->thread;
    SDL_UnlockMutex(resolvermutex);
    if(!thread || SDL_GetThreadID(thread) != SDL_ThreadID())
        return 0;
    while(thread == rt->thread)
    {
        SDL_LockMutex(resolvermutex);
        while(resolverqueries.empty()) SDL_CondWait(querycond, resolvermutex);
        rt->query = resolverqueries.pop();
        rt->starttime = lastmillis;
        SDL_UnlockMutex(resolvermutex);

        ENetAddress address = { ENET_HOST_ANY, sv->serverinfoport() };
        enet_address_set_host(&address, rt->query);

        SDL_LockMutex(resolvermutex);
        if(thread == rt->thread)
        {
            resolverresult &rr = resolverresults.add();
            rr.query = rt->query;
            rr.address = address;
            rt->query = NULL;
            rt->starttime = 0;
            SDL_CondSignal(resultcond);
        };
        SDL_UnlockMutex(resolvermutex);
    };
    return 0;
};

void resolverinit()
{
    resolvermutex = SDL_CreateMutex();
    querycond = SDL_CreateCond();
    resultcond = SDL_CreateCond();

    SDL_LockMutex(resolvermutex);
    loopi(RESOLVERTHREADS)
    {
        resolverthread &rt = resolverthreads.add();
        rt.query = NULL;
        rt.starttime = 0;
        rt.thread = SDL_CreateThread(resolverloop, &rt);
    };
    SDL_UnlockMutex(resolvermutex);
};

void resolverstop(resolverthread &rt)
{
    SDL_LockMutex(resolvermutex);
    if(rt.query)
    {
#ifndef __APPLE__
        SDL_KillThread(rt.thread);
#endif
        rt.thread = SDL_CreateThread(resolverloop, &rt);
    };
    rt.query = NULL;
    rt.starttime = 0;
    SDL_UnlockMutex(resolvermutex);
}; 

void resolverclear()
{
    if(resolverthreads.empty()) return;

    SDL_LockMutex(resolvermutex);
    resolverqueries.setsize(0);
    resolverresults.setsize(0);
    loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        resolverstop(rt);
    };
    SDL_UnlockMutex(resolvermutex);
};

void resolverquery(const char *name)
{
    if(resolverthreads.empty()) resolverinit();

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    SDL_UnlockMutex(resolvermutex);
};

bool resolvercheck(const char **name, ENetAddress *address)
{
    bool resolved = false;
    SDL_LockMutex(resolvermutex);
    if(!resolverresults.empty())
    {
        resolverresult &rr = resolverresults.pop();
        *name = rr.query;
        address->host = rr.address.host;
        resolved = true;
    }
    else loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        if(rt.query && lastmillis - rt.starttime > RESOLVERLIMIT)        
        {
            resolverstop(rt);
            *name = rt.query;
            resolved = true;
        };    
    };
    SDL_UnlockMutex(resolvermutex);
    return resolved;
};

bool resolverwait(const char *name, ENetAddress *address)
{
    if(resolverthreads.empty()) resolverinit();

    s_sprintfd(text)("resolving %s... (esc to abort)", name);
    show_out_of_renderloop_progress(0, text);

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    int starttime = SDL_GetTicks(), timeout = 0;
    bool resolved = false;
    for(;;) 
    {
        SDL_CondWaitTimeout(resultcond, resolvermutex, 250);
        loopv(resolverresults) if(resolverresults[i].query == name) 
        {
            address->host = resolverresults[i].address.host;
            resolverresults.remove(i);
            resolved = true;
            break;
        };
        if(resolved) break;
    
        timeout = SDL_GetTicks() - starttime;
        show_out_of_renderloop_progress(min(float(timeout)/RESOLVERLIMIT, 1), text);
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) timeout = RESOLVERLIMIT + 1;
        };

        if(timeout > RESOLVERLIMIT) break;    
    };
    if(!resolved && timeout > RESOLVERLIMIT)
    {
        loopv(resolverthreads)
        {
            resolverthread &rt = resolverthreads[i];
            if(rt.query == name) { resolverstop(rt); break; };
        };
    };
    SDL_UnlockMutex(resolvermutex);
    return resolved;
};

enum { UNRESOLVED = 0, RESOLVING, RESOLVED };

struct serverinfo
{
    string name;
    string full;
    string map;
    string sdesc;
    int numplayers, ping, resolved;
    vector<int> attr;
    ENetAddress address;
};

vector<serverinfo> servers;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

char *getservername(int n) { return servers[n].name; };

void addserver(char *servername)
{
    loopv(servers) if(!strcmp(servers[i].name, servername)) return;
    serverinfo &si = servers.add();
    s_strcpy(si.name, servername);
    si.full[0] = 0;
    si.ping = 999;
    si.map[0] = 0;
    si.sdesc[0] = 0;
    si.resolved = UNRESOLVED;
    si.address.host = ENET_HOST_ANY;
    si.address.port = sv->serverinfoport();
};

void pingservers()
{
    if(pingsock == ENET_SOCKET_NULL) pingsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, NULL);
    ENetBuffer buf;
    uchar ping[MAXTRANS];
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.address.host == ENET_HOST_ANY) continue;
        ucharbuf p(ping, sizeof(ping));
        putint(p, lastmillis);
        buf.data = ping;
        buf.dataLength = p.length();
        enet_socket_send(pingsock, &si.address, &buf, 1);
    };
    lastinfo = lastmillis;
};
  
void checkresolver()
{
    int resolving = 0;
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.resolved == RESOLVED) continue;
        if(si.address.host == ENET_HOST_ANY)
        {
            if(si.resolved == UNRESOLVED) { si.resolved = RESOLVING; resolverquery(si.name); };
            resolving++;
        };
    };
    if(!resolving) return;

    const char *name = NULL;
    ENetAddress addr = { ENET_HOST_ANY, sv->serverinfoport() };
    while(resolvercheck(&name, &addr))
    {
        loopv(servers)
        {
            serverinfo &si = servers[i];
            if(name == si.name)
            {
                si.resolved = RESOLVED; 
                si.address = addr;
                addr.host = ENET_HOST_ANY;
                break;
            };
        };
    };
};

void checkpings()
{
    if(pingsock==ENET_SOCKET_NULL) return;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    ENetBuffer buf;
    ENetAddress addr;
    uchar ping[MAXTRANS];
    char text[MAXTRANS];
    buf.data = ping; 
    buf.dataLength = sizeof(ping);
    while(enet_socket_wait(pingsock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(pingsock, &addr, &buf, 1);
        if(len <= 0) return;  
        loopv(servers)
        {
            serverinfo &si = servers[i];
            if(addr.host == si.address.host)
            {
                ucharbuf p(ping, len);
                si.ping = lastmillis - getint(p);
                si.numplayers = getint(p);
                int numattr = getint(p);
                si.attr.setsize(0);
                loopj(numattr) si.attr.add(getint(p));
                getstring(text, p);
                s_strcpy(si.map, text);
                getstring(text, p);
                s_strcpy(si.sdesc, text);                
                break;
            };
        };
    };
};

int sicompare(serverinfo *a, serverinfo *b)
{
    bool ac = sv->servercompatible(a->name, a->sdesc, a->map, a->ping, a->attr, a->numplayers),
         bc = sv->servercompatible(b->name, b->sdesc, b->map, b->ping, b->attr, b->numplayers);
    if(ac>bc) return -1;
    if(bc>ac) return 1;   
    if(a->ping>b->ping) return 1;
    if(a->ping<b->ping) return -1;
    return strcmp(a->name, b->name);
};

void refreshservers()
{
    static int lastrefresh = 0;
    if(lastrefresh==lastmillis) return;
    lastrefresh = lastmillis;

    checkresolver();
    checkpings();
    if(lastmillis - lastinfo >= 5000) pingservers();
    servers.sort(sicompare);
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.address.host != ENET_HOST_ANY && si.ping != 999)
        {
            sv->serverinfostr(si.full, si.name, si.sdesc, si.map, si.ping, si.attr, si.numplayers);
        }
        else
        {
            s_sprintf(si.full)(si.address.host != ENET_HOST_ANY ? "[waiting for response] %s" : "[unknown host] %s\t", si.name);
        };
        si.full[60] = 0; // cut off too long server descriptions
    };
};

const char *showservers(g3d_gui *cgui)
{
    refreshservers();
    const char *name = NULL;
    int maxmenu = 20;
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(cgui->button(si.full, 0xFFFFDD, "server")&G3D_UP) name = si.name;
        if(!--maxmenu) break;
    };
    return name;
};

void updatefrommaster()
{
    const int MAXUPD = 32000;
    uchar buf[MAXUPD];
    uchar *reply = retrieveservers(buf, MAXUPD);
    if(!*reply || strstr((char *)reply, "<html>") || strstr((char *)reply, "<HTML>")) conoutf("master server not replying");
    else
    {
        resolverclear();
        servers.setsize(0);
        execute((char *)reply);
    };
    refreshservers();
};

COMMAND(addserver, "s");
COMMAND(updatefrommaster, "");

void writeservercfg()
{
    FILE *f = fopen("servers.cfg", "w");
    if(!f) return;
    fprintf(f, "// servers connected to are added here automatically\n\n");
    loopvrev(servers) fprintf(f, "addserver %s\n", servers[i].name);
    fclose(f);
};

