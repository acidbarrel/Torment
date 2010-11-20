// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "pch.h"
#include "engine.h" 

#ifdef STANDALONE
void localservertoclient(int chan, uchar *buf, int len) {};
void fatal(char *s, char *o) { cleanupserver(); printf("servererror: %s\n", s); exit(1); };
#endif

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

void putint(ucharbuf &p, int n)
{
    if(n<128 && n>-127) p.put(n);
    else if(n<0x8000 && n>=-0x8000) { p.put(0x80); p.put(n); p.put(n>>8); }
    else { p.put(0x81); p.put(n); p.put(n>>8); p.put(n>>16); p.put(n>>24); };
};

int getint(ucharbuf &p)
{
    int c = (char)p.get();
    if(c==-128) { int n = p.get(); n |= char(p.get())<<8; return n; }
    else if(c==-127) { int n = p.get(); n |= p.get()<<8; n |= p.get()<<16; return n|(p.get()<<24); } 
    else return c;
};

// much smaller encoding for unsigned integers up to 28 bits, but can handle signed
void putuint(ucharbuf &p, int n)
{
    if(n < 0 || n >= (1<<21))
    {
        p.put(0x80 | (n & 0x7F));
        p.put(0x80 | ((n >> 7) & 0x7F));
        p.put(0x80 | ((n >> 14) & 0x7F));
        p.put(n >> 21);
    }
    else if(n < (1<<7)) p.put(n);
    else if(n < (1<<14))
    {
        p.put(0x80 | (n & 0x7F));
        p.put(n >> 7);
    }
    else 
    { 
        p.put(0x80 | (n & 0x7F)); 
        p.put(0x80 | ((n >> 7) & 0x7F));
        p.put(n >> 14); 
    };
};

int getuint(ucharbuf &p)
{
    int n = p.get();
    if(n & 0x80)
    {
        n += (p.get() << 7) - 0x80;
        if(n & (1<<14)) n += (p.get() << 14) - (1<<14);
        if(n & (1<<21)) n += (p.get() << 21) - (1<<21);
        if(n & (1<<28)) n |= 0xF0000000; 
    };
    return n;
};

void sendstring(const char *t, ucharbuf &p)
{
    while(*t) putint(p, *t++);
    putint(p, 0);
};

void getstring(char *text, ucharbuf &p, int len)
{
    char *t = text;
    do
    {
        if(t>=&text[len]) { text[len-1] = 0; return; };
        if(!p.remaining()) { *t = 0; return; }; 
        *t = getint(p);
    }
    while(*t++);
};

enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

struct client                   // server side version of "dynent" type
{
    int type;
    int num;
    ENetPeer *peer;
    string hostname;
    void *info;
};

vector<client *> clients;

ENetHost *serverhost = NULL;
size_t bsend = 0, brec = 0;
int laststatus = 0; 
ENetSocket pongsock = ENET_SOCKET_NULL;

void sendfile(int cn, int chan, FILE *file)
{
#ifndef STANDALONE
    extern ENetHost *clienthost;
#endif
    if(cn < 0)
    {
#ifndef STANDALONE
        if(!clienthost || clienthost->peers[0].state != ENET_PEER_STATE_CONNECTED) 
#endif
            return;
    }
    else if(cn >= clients.length() || clients[cn]->type != ST_TCPIP) return;

    fseek(file, 0, SEEK_END);
    int len = ftell(file);
    ENetPacket *packet = enet_packet_create(NULL, len, ENET_PACKET_FLAG_RELIABLE);
    rewind(file);
    fread(packet->data, 1, len, file);

    if(cn >= 0) enet_peer_send(clients[cn]->peer, chan, packet);
#ifndef STANDALONE
    else enet_peer_send(&clienthost->peers[0], chan, packet);
#endif

    if(!packet->referenceCount) enet_packet_destroy(packet);
};

void process(ENetPacket *packet, int sender, int chan);
void multicast(ENetPacket *packet, int sender, int chan);
//void disconnect_client(int n, int reason);

void *getinfo(int i)    { return !clients.inrange(i) || clients[i]->type==ST_EMPTY ? NULL : clients[i]->info; };
int getnbluelients()     { return clients.length(); };
uint getclientip(int n) { return clients.inrange(n) && clients[n]->type==ST_TCPIP ? clients[n]->peer->address.host : 0; };

void sendpacket(int n, int chan, ENetPacket *packet)
{
    switch(clients[n]->type)
    {
        case ST_TCPIP:
        {
            enet_peer_send(clients[n]->peer, chan, packet);
            bsend += packet->dataLength;
            break;
        };

        case ST_LOCAL:
            localservertoclient(chan, packet->data, (int)packet->dataLength);
            break;
    };
};

void sendf(int cn, int chan, const char *format, ...)
{
    bool reliable = false;
    if(*format=='r') { reliable = true; ++format; };
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    ucharbuf p(packet->data, packet->dataLength);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'i': 
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        };
        case 's': sendstring(va_arg(args, const char *), p); break;
    };
    va_end(args);
    enet_packet_resize(packet, p.length());
    if(cn<0) multicast(packet, -1, chan);
    else sendpacket(cn, chan, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

char *disc_reasons[] = { "normal", "end of packet", "client num", "kicked/banned", "tag type", "ip is banned", "server is in private mode", "server FULL (maxclients)" };

void disconnect_client(int n, int reason)
{
    if(clients[n]->type!=ST_TCPIP) return;
    s_sprintfd(s)("(%s) Disconnected because: %s\n", clients[n]->hostname, disc_reasons[reason]);
    puts(s);
    enet_peer_disconnect(clients[n]->peer, reason);
    sv->clientdisconnect(n);
    clients[n]->type = ST_EMPTY;
    clients[n]->peer->data = NULL;
    sv->sendservmsg(s);
};

void process(ENetPacket *packet, int sender, int chan)   // sender may be -1
{
    ucharbuf p(packet->data, (int)packet->dataLength);
    sv->parsepacket(sender, chan, (packet->flags&ENET_PACKET_FLAG_RELIABLE)!=0, p);
    if(p.overread()) { disconnect_client(sender, DISC_EOP); return; };
};

void send_welcome(int n)
{
    ENetPacket *packet = enet_packet_create (NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    int chan = sv->welcomepacket(p, n);
    enet_packet_resize(packet, p.length());
    sendpacket(n, chan, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void multicast(ENetPacket *packet, int sender, int chan)
{
    loopv(clients)
    {
        if(i==sender) continue;
        sendpacket(i, chan, packet);
    };
};

void localclienttoserver(int chan, ENetPacket *packet)
{
    process(packet, 0, chan);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

client &addclient()
{
    loopv(clients) if(clients[i]->type==ST_EMPTY)
    {
        sv->resetinfo(clients[i]->info);
        return *clients[i];
    };
    client *c = new client;
    c->num = clients.length();
    c->info = sv->newinfo();
    clients.add(c);
    return *c;
};

int nonlocalclients = 0;

bool hasnonlocalclients() { return nonlocalclients!=0; };

void sendpongs()        // reply all server info requests
{
    ENetBuffer buf;
    ENetAddress addr;
    uchar pong[MAXTRANS];
    int len;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    buf.data = pong;
    while(enet_socket_wait(pongsock, &events, 0) >= 0 && events)
    {
        buf.dataLength = sizeof(pong);
        len = enet_socket_receive(pongsock, &addr, &buf, 1);
        if(len < 0) return;
        ucharbuf p(&pong[len], sizeof(pong)-len);
        sv->serverinforeply(p);
        buf.dataLength = len + p.length();
        enet_socket_send(pongsock, &addr, &buf, 1);
    };
};      

#ifdef STANDALONE
bool resolverwait(const char *name, ENetAddress *address)
{
    return enet_address_set_host(address, name) >= 0;
};
#endif

ENetSocket mssock = ENET_SOCKET_NULL;

void httpgetsend(ENetAddress &ad, char *hostname, char *req, char *ref, char *agent)
{
    if(mssock!=ENET_SOCKET_NULL)
    {
        enet_socket_destroy(mssock);
        mssock = ENET_SOCKET_NULL;
    };
    if(ad.host==ENET_HOST_ANY)
    {
		//We're not going to spam the server console, with nonsense... (there is no ms)
        //printf("looking up %s...\n", hostname);
        if(!resolverwait(hostname, &ad)) return;
    };
    mssock = enet_socket_create(ENET_SOCKET_TYPE_STREAM, NULL);
    if(mssock==ENET_SOCKET_NULL) { printf("could not open socket\n"); return; };
    if(enet_socket_connect(mssock, &ad)<0) 
    { 
		//Same message applies. ENET_HOST_ANY
        //printf("could not connect\n"); 
        enet_socket_destroy(mssock);
        mssock = ENET_SOCKET_NULL;
        return; 
    };
    ENetBuffer buf;
    s_sprintfd(httpget)("GET %s HTTP/1.0\nHost: %s\nReferer: %s\nUser-Agent: %s\n\n", req, hostname, ref, agent);
    buf.data = httpget;
    buf.dataLength = strlen((char *)buf.data);
    printf("sending request to %s...\n", hostname);
    enet_socket_send(mssock, NULL, &buf, 1);
};  

void httpgetreceive(ENetBuffer &buf)
{
    if(mssock==ENET_SOCKET_NULL) return;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    if(enet_socket_wait(mssock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(mssock, NULL, &buf, 1);
        if(len<=0)
        {
            enet_socket_destroy(mssock);
            mssock = ENET_SOCKET_NULL;
            return;
        };
        buf.data = ((char *)buf.data)+len;
        ((char*)buf.data)[0] = 0;
        buf.dataLength -= len;
    };
};  

uchar *stripheader(uchar *b)
{
    char *s = strstr((char *)b, "\n\r\n");
    if(!s) s = strstr((char *)b, "\n\n");
    return s ? (uchar *)s : b;
};

ENetAddress masterserver = { ENET_HOST_ANY, 80 };
int updmaster = 0;
string masterbase;
string masterpath;
uchar masterrep[MAXTRANS];
ENetBuffer masterb;

void updatemasterserver()
{
	//There is no master for Torment Alpha Trails
    //s_sprintfd(path)("%sregister.do?action=add", masterpath);
    //httpgetsend(masterserver, masterbase, path, sv->servername(), sv->servername());
    masterrep[0] = 0;
    masterb.data = masterrep;
    masterb.dataLength = MAXTRANS-1;
}; 

void checkmasterreply()
{
    bool busy = mssock!=ENET_SOCKET_NULL;
    httpgetreceive(masterb);
	//printf("masterserver reply: %s\n",
    if(busy && mssock==ENET_SOCKET_NULL) printf("", stripheader(masterrep));
}; 

uchar *retrieveservers(uchar *buf, int buflen)
{
    s_sprintfd(path)("%sretrieve.do?item=list", masterpath);
    httpgetsend(masterserver, masterbase, path, sv->servername(), sv->servername());
    ENetBuffer eb;
    buf[0] = 0;
    eb.data = buf;
    eb.dataLength = buflen-1;
    while(mssock!=ENET_SOCKET_NULL) httpgetreceive(eb);
    return stripheader(buf);
};

#define DEFAULTCLIENTS 6

int uprate = 0, maxclients = DEFAULTCLIENTS;
char *sdesc = "", *ip = "", *master = NULL, *adminpass = NULL;
char *game = "fps";

void serverslice(int seconds, uint timeout)   // main server update, called from main loop in sp, or from below in dedicated server
{
    sv->serverupdate(seconds);

    if(!serverhost) return;     // below is network only

    sendpongs();
    
    if(*masterpath) checkmasterreply();

    if(seconds>updmaster && *masterpath)       // send alive signal to masterserver every hour of uptime
    {
        updatemasterserver();
        updmaster = seconds+60*60;
    };
    
    nonlocalclients = 0;
    loopv(clients) if(clients[i]->type==ST_TCPIP) nonlocalclients++;

    if(seconds-laststatus>60)   // display bandwidth stats, useful for server ops
    {
        laststatus = seconds;     
        if(nonlocalclients || bsend || brec) printf("status: %d remote clients, %.1f send, %.1f rec (K/sec)\n", nonlocalclients, bsend/60.0f/1024, brec/60.0f/1024);
        bsend = brec = 0;
    };

    ENetEvent event;
    if(enet_host_service(serverhost, &event, timeout) > 0)
    switch(event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
        {
            client &c = addclient();
            c.type = ST_TCPIP;
            c.peer = event.peer;
            c.peer->data = &c;
            char hn[1024];
            s_strcpy(c.hostname, (enet_address_get_host(&c.peer->address, hn, sizeof(hn))==0) ? hn : "unknown");
            printf("(%s) Has connected.\n", c.hostname);
            int reason = DISC_MAXCLIENTS;
            if(nonlocalclients<maxclients && !(reason = sv->clientconnect(c.num, c.peer->address.host))) send_welcome(c.num);
            else disconnect_client(c.num, reason);
            break;
        };
        case ENET_EVENT_TYPE_RECEIVE:
        {
            brec += event.packet->dataLength;
            client *c = (client *)event.peer->data;
            if(c) process(event.packet, c->num, event.channelID);
            if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
            break;
        };
        case ENET_EVENT_TYPE_DISCONNECT: 
        {
            client *c = (client *)event.peer->data;
            if(!c) break;
            printf("(%s) Has disconnected.\n", c->hostname);
            sv->clientdisconnect(c->num);
            c->type = ST_EMPTY;
            event.peer->data = NULL;
            break;
        };
        default:
            break;
    };
    if(sv->sendpackets()) enet_host_flush(serverhost);
};

void cleanupserver()
{
    if(serverhost) enet_host_destroy(serverhost);
};

void localdisconnect()
{
    loopv(clients) if(clients[i]->type==ST_LOCAL) 
    {
        sv->localdisconnect(i);
        clients[i]->type = ST_EMPTY;
    };
};

void localconnect()
{
    client &c = addclient();
    c.type = ST_LOCAL;
    s_strcpy(c.hostname, "local");
    sv->localconnect(c.num);
    send_welcome(c.num); 
};

hashtable<char *, igame *> *gamereg = NULL;

void registergame(char *name, igame *ig)
{
    if(!gamereg) gamereg = new hashtable<char *, igame *>;
    (*gamereg)[name] = ig;
};

igameclient     *cl = NULL;
igameserver     *sv = NULL;
iclientcom      *cc = NULL;
icliententities *et = NULL;

void initgame(char *game)
{
    igame **ig = gamereg->access(game);
    if(!ig) fatal("cannot start game module: ", game);
    sv = (*ig)->newserver();
    cl = (*ig)->newclient();
    if(cl)
    {
        cc = cl->getcom();
        et = cl->getents();
        cl->initclient();
    };
}

void initserver(bool dedicated)
{
    initgame(game);
    
    if(!master) master = sv->getdefaultmaster();
    char *mid = strstr(master, "/");
    if(!mid) mid = master;
    s_strcpy(masterpath, mid);
    s_strncpy(masterbase, master, mid-master+1);

    if(dedicated)
    {
        ENetAddress address = { ENET_HOST_ANY, sv->serverport() };
        if(*ip && enet_address_set_host(&address, ip)<0) printf("WARNING: server ip not resolved");
        serverhost = enet_host_create(&address, maxclients+1, 0, uprate);
        if(!serverhost) fatal("could not create server host\n");
        loopi(maxclients) serverhost->peers[i].data = (void *)-1;
        address.port = sv->serverinfoport();
        pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, &address);
        if(pongsock == ENET_SOCKET_NULL) fatal("could not create server info socket\n");
    };

    sv->serverinit(sdesc, adminpass);

    if(dedicated)       // do not return, this becomes main loop
    {
        #ifdef WIN32
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        #endif
		printf(".########..#######..########..##.....##.########.##....##.########\n....##....##.....##.##.....##.###...###.##.......###...##....##...\n....##....##.....##.##.....##.####.####.##.......####..##....##...\n....##....##.....##.########..##.###.##.######...##.##.##....##...\n....##....##.....##.##...##...##.....##.##.......##..####....##...\n....##....##.....##.##....##..##.....##.##.......##...###....##...\n....##.....#######..##.....##.##.....##.########.##....##....##...\nVersion 0.0.12.48\n\n\n... The Alpha Trails Server ...\n\n");
        printf("Server is online.\n");
        atexit(cleanupserver);
        atexit(enet_deinitialize);
        for(;;) serverslice(time(NULL), 5);
    };
};

bool serveroption(char *opt)
{
    switch(opt[1])
    {
        case 'u': uprate = atoi(opt+2); return true;
        case 'c': 
        {
            int clients = atoi(opt+2); 
            if(clients > 0) maxclients = min(clients, MAXCLIENTS);
            else maxclients = DEFAULTCLIENTS;
            return true;
        };
        case 'n': sdesc = opt+2; return true;
        case 'i': ip = opt+2; return true;
        case 'm': master = opt+2; return true;
        case 'g': game = opt+2; return true;
        case 'p': adminpass = opt+2; return true;
        default: return false;
    };
    
};

#ifdef STANDALONE
int main(int argc, char* argv[])
{   
    for(int i = 1; i<argc; i++) if(argv[i][0]!='-' || !serveroption(argv[i])) printf("WARNING: unknown commandline option\n");
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    initserver(true);
    return 0;
};
#endif
