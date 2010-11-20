struct fpsserver : igameserver
{
    #define CAPTURESERV 1
    #include "capture.h"
    #undef CAPTURESERV

    struct server_entity            // server side version of "entity" type
    {
        int type;
        int spawnsecs;
        char spawned;
    };

    struct clientscore
    {
        int maxhealth, frags, timeplayed;
 
        void reset() 
        { 
            maxhealth = 100;
            frags = timeplayed = 0; 
        };
    };

    struct savedscore : clientscore
    {
        uint ip;
        string name;
    };

    struct clientinfo
    {
        int clientnum;
        string name, team, mapvote;
        int modevote;
        bool master, spectator, local;
        vec o;
        int state;
        clientscore score;
        enet_uint32 gamestart;
        vector<uchar> position, messages;

        clientinfo() { reset(); };

        void mapchange()
        {
            score.reset();
            mapvote[0] = 0;
            o = vec(-1e10f, -1e10f, -1e10f);
            state = -1;
        };

        void reset()
        {
            name[0] = team[0] = 0;
            master = spectator = local = false;
            position.setsizenodelete(0);
            messages.setsizenodelete(0);
            mapchange();
        };
    };

    struct worldstate
    {
        enet_uint32 uses;
        vector<uchar> positions, messages;
    };

    struct ban
    {
        int time;
        uint ip;
    };
    
    bool notgotitems, notgotbases;        // true when map has changed and waiting for clients to send item
    int gamemode;

    string serverdesc;
    string smapname;
    int interm, minremain, mapend;
    bool mapreload;
    int lastsec;
    enet_uint32 lastsend;
    int mastermode;
    int currentmaster;
    bool masterupdate;
    string masterpass;
    FILE *mapdata;

    vector<ban> bannedips;
    vector<clientinfo *> clients;
    vector<worldstate *> worldstates;
    bool reliablemessages;

    captureserv cps;

    enum { MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE };

    fpsserver() : notgotitems(true), notgotbases(false), gamemode(0), interm(0), minremain(0), mapend(0), mapreload(false), lastsec(0), lastsend(0), mastermode(MM_OPEN), currentmaster(-1), masterupdate(false), mapdata(NULL), reliablemessages(false), cps(*this) {};

    void *newinfo() { return new clientinfo; };
    void resetinfo(void *ci) { ((clientinfo *)ci)->reset(); }; 
    
    vector<server_entity> sents;
    vector<savedscore> scores;

    static const char *modestr(int n)
    {
        static const char *modenames[] =
        {
            "SP", "DMSP", "ffa/default", "coopedit", "ffa/duel", "teamplay",
            "instagib", "instagib team", "efficiency", "efficiency team",
            "insta arena", "insta clan arena", "Rocket Arena", "Team Rocket Arena",
            "capture",
        };
        return (n>=-2 && size_t(n+2)<sizeof(modenames)/sizeof(modenames[0])) ? modenames[n+2] : "unknown";
    };

    static char msgsizelookup(int msg)
    {
        static char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
        { 
            SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 0, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
            SV_DIED, 4, SV_DAMAGE, 7, SV_SHOT, 8, SV_FRAGS, 2, SV_GUNSELECT, 2,
            SV_MAPCHANGE, 0, SV_MAPVOTE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
            SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2,
            SV_TIMEUP, 2, SV_MAPRELOAD, 1, SV_ITEMACC, 2,
            SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_RESUME, 4,
            SV_EDITENT, 10, SV_EDITF, 16, SV_EDITT, 16, SV_EDITM, 15, SV_FLIP, 14, SV_COPY, 14, SV_PASTE, 14, SV_ROTATE, 15, SV_REPLACE, 16, SV_DELCUBE, 14, SV_NEWMAP, 2, SV_GETMAP, 1,
            SV_MASTERMODE, 2, SV_KICK, 2, SV_CURRENTMASTER, 2, SV_SPECTATOR, 3, SV_SETMASTER, 0, SV_SETTEAM, 0,
            SV_BASES, 0, SV_BASEINFO, 0, SV_TEAMSCORE, 0, SV_REPAMMO, 4, SV_FORCEINTERMISSION, 1,  SV_ANNOUNCE, 2,
            SV_CLIENT, 0,
            -1
        };
        for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
        return -1;
    };

    void sendservmsg(const char *s) { sendf(-1, 1, "ris", SV_SERVMSG, s); };

    void resetitems() { sents.setsize(0); cps.reset(); };

    void pickup(int i, int sec, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if(!sents.inrange(i)) return;
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(ci && ci->state==CS_ALIVE && sents[i].spawned)
        {
            sents[i].spawned = false;
            if(sents[i].type==I_QUAD) sec += rnd(40)-20;
            sents[i].spawnsecs = sec;
            sendf(sender, 1, "ri2", SV_ITEMACC, i);
            //if(minremain>=0 && sents[i].type == pr) ci->score.maxhealth += 10;
        };
    };

    void vote(char *map, int reqmode, int sender)
    {
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(ci->spectator && !ci->master) return;
        s_strcpy(ci->mapvote, map);
        ci->modevote = reqmode;
        if(!ci->mapvote[0]) return;
        if(ci->local || mapreload || (ci->master && mastermode>=MM_VETO))
        {
            if(!ci->local && !mapreload) 
            {
                s_sprintfd(msg)("master forced %s on map %s", modestr(reqmode), map);
                sendservmsg(msg);
            };
            sendf(-1, 1, "risi", SV_MAPCHANGE, ci->mapvote, ci->modevote);
            changemap(ci->mapvote, ci->modevote);
        }
        else 
        {
            s_sprintfd(msg)("%s suggests %s on map %s (select map to vote)", ci->name, modestr(reqmode), map);
            sendservmsg(msg);
            checkvotes();
        };
    };

    clientinfo *choosebestclient(float &bestrank)
    {
        clientinfo *best = NULL;
        bestrank = -1;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->score.timeplayed<0) continue;
            float rank = float(ci->score.frags)/float(max(ci->score.timeplayed, 1));
            if(!best || rank > bestrank) { best = ci; bestrank = rank; };
        };
        if(best) best->score.timeplayed = -1;
        return best;
    };  

    void autoteam()
    {
        const char *teamnames[2] = {"red", "blue"};
        vector<clientinfo *> team[2];
        float teamrank[2] = {0, 0};
        for(int round = 0, remaining = clients.length(); remaining; round++)
        {
            int first = round&1, second = (round+1)&1, selected = 0;
            while(teamrank[first] <= teamrank[second])
            {
                float rank;
                clientinfo *ci = choosebestclient(rank);
                if(!ci) break;
                if(selected && rank<=0) break;    
                team[first].add(ci);
                teamrank[first] += rank;
                selected++;
                if(rank<=0) break;
            };
            remaining -= selected;
        };
        loopi(sizeof(team)/sizeof(team[0]))
        {
            loopvj(team[i])
            {
                clientinfo *ci = team[i][j];
                if(!strcmp(ci->team, teamnames[i])) continue;
                s_strncpy(ci->team, teamnames[i], MAXTEAMLEN+1);
                sendf(-1, 1, "riis", SV_SETTEAM, ci->clientnum, teamnames[i]);
            };
        };
    };

    struct teamscore
    {
        const char *team;
        float rank;
        int clients;
    };

    const char *chooseworstteam(const char *suggest)
    {
        vector<teamscore> teamscores;
        enet_uint32 curtime = enet_time_get();
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->team[0]) continue;
            ci->score.timeplayed += curtime - ci->gamestart;
            ci->gamestart = curtime;
            teamscore *ts = NULL;
            loopvj(teamscores) if(!strcmp(teamscores[j].team, ci->team)) { ts = &teamscores[j]; break; };
            if(!ts) { ts = &teamscores.add(); ts->team = ci->team; ts->rank = 0; ts->clients = 0; };
            ts->rank += float(ci->score.frags)/float(max(ci->score.timeplayed, 1));
            ts->clients++;
        };
        if(teamscores.length()==1)
        {
            if(suggest[0] && strcmp(teamscores[0].team, suggest)) return NULL;
            return strcmp(teamscores[0].team, "red") ? "red" : "blue";
        };
        teamscore *worst = NULL;
        loopv(teamscores)
        {
            teamscore &ts = teamscores[i];
            if(!worst || ts.rank < worst->rank || (ts.rank == worst->rank && ts.clients < worst->clients)) worst = &ts;
        };
        return worst ? worst->team : NULL;
    };

    void changemap(const char *s, int mode)
    {
        mapreload = false;
        gamemode = mode;
        minremain = m_teammode ? 15 : 10;
        mapend = lastsec+minremain*60;
        interm = 0;
        s_strcpy(smapname, s);
        resetitems();
        notgotitems = true;
        notgotbases = m_capture;
        scores.setsize(0);
        enet_uint32 gamestart = enet_time_get();
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->score.timeplayed += gamestart - ci->gamestart;
        };
        if(m_teammode) autoteam();
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->mapchange();
            ci->gamestart = gamestart;
        };
    };

    clientscore &findscore(clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip) return *(clientscore *)0;
        if(!insert) loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->clientnum != ci->clientnum && getclientip(oi->clientnum) == ip && !strcmp(oi->name, ci->name))
            {
                enet_uint32 gamestart = enet_time_get();
                oi->score.timeplayed += gamestart - oi->gamestart;
                oi->gamestart = gamestart;
                return oi->score;
            };
        };
        loopv(scores)
        {
            savedscore &sc = scores[i];
            if(sc.ip == ip && !strcmp(sc.name, ci->name)) return sc;
        };
        if(!insert) return *(clientscore *)0;
        savedscore &sc = scores.add();
        sc.reset();
        sc.ip = ip;
        s_strcpy(sc.name, ci->name);
        return sc;
    };

    void savescore(clientinfo *ci)
    {
        clientscore &sc = findscore(ci, true);
        if(&sc) sc = ci->score;
    };

    struct votecount
    {
        char *map;
        int mode, count;
        votecount() {};
        votecount(char *s, int n) : map(s), mode(n), count(0) {};
    };

    void checkvotes(bool force = false)
    {
        vector<votecount> votes;
        int maxvotes = 0;
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->spectator && !oi->master) continue;
            maxvotes++;
            if(!oi->mapvote[0]) continue;
            votecount *vc = NULL;
            loopvj(votes) if(!strcmp(oi->mapvote, votes[j].map) && oi->modevote==votes[j].mode)
            { 
                vc = &votes[j];
                break;
            };
            if(!vc) vc = &votes.add(votecount(oi->mapvote, oi->modevote));
            vc->count++;
        };
        votecount *best = NULL;
        loopv(votes) if(!best || votes[i].count > best->count || (votes[i].count == best->count && rnd(2))) best = &votes[i];
        if(force || (best && best->count > maxvotes/2))
        {
            if(best) 
            { 
                sendservmsg(force ? "vote passed by default" : "vote passed by majority");
                sendf(-1, 1, "risi", SV_MAPCHANGE, best->map, best->mode);
                changemap(best->map, best->mode); 
            }
            else if(clients.length()) 
            {
                mapreload = true;
                sendf(-1, 1, "ri", SV_MAPRELOAD);
            };
        };
    };

    int checktype(int type, clientinfo *ci)
    {
        if(ci && ci->local) return type;
        // spectators can only connect and talk
        static int spectypes[] = { SV_INITC2S, SV_POS, SV_TEXT, SV_PING, SV_CLIENTPING, SV_GETMAP };
        if(ci && ci->spectator && !ci->master)
        {
            loopi(sizeof(spectypes)/sizeof(int)) if(type == spectypes[i]) return type;
            return -1;
        };
        // only allow edit messages in coop-edit mode
        if(type>=SV_EDITENT && type<=SV_GETMAP && gamemode!=1) return -1;
        // server only messages
        static int servtypes[] = { SV_INITS2C, SV_MAPRELOAD, SV_SERVMSG, SV_ITEMACC, SV_ITEMSPAWN, SV_TIMEUP, SV_CDIS, SV_CURRENTMASTER, SV_PONG, SV_RESUME, SV_TEAMSCORE, SV_BASEINFO, SV_ANNOUNCE, SV_CLIENT };
        if(ci) loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
        return type;
    };

    static void freecallback(ENetPacket *packet)
    {
        extern igameserver *sv;
        ((fpsserver *)sv)->cleanworldstate(packet);
    };

    void cleanworldstate(ENetPacket *packet)
    {
        loopv(worldstates)
        {
            worldstate *ws = worldstates[i];
            if(packet->data >= ws->positions.getbuf() && packet->data <= &ws->positions.last()) ws->uses--;
            else if(packet->data >= ws->messages.getbuf() && packet->data <= &ws->messages.last()) ws->uses--;
            else continue;
            if(!ws->uses)
            {
                delete ws;
                worldstates.remove(i);
            };
            break;
        };
    };

    bool buildworldstate()
    {
        static struct { int posoff, msgoff, msglen; } pkt[MAXCLIENTS];
        worldstate &ws = *new worldstate;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.position.empty()) pkt[i].posoff = -1;
            else
            {
                pkt[i].posoff = ws.positions.length();
                loopvj(ci.position) ws.positions.add(ci.position[j]);
            };
            if(ci.messages.empty()) pkt[i].msgoff = -1;
            else
            {
                pkt[i].msgoff = ws.messages.length();
                ucharbuf p = ws.messages.reserve(16);
                putint(p, SV_CLIENT);
                putint(p, ci.clientnum);
                putuint(p, ci.messages.length());
                ws.messages.addbuf(p);
                loopvj(ci.messages) ws.messages.add(ci.messages[j]);
                pkt[i].msglen = ws.messages.length() - pkt[i].msgoff;
            };
        };
        int psize = ws.positions.length(), msize = ws.messages.length();
        loopi(psize) { uchar c = ws.positions[i]; ws.positions.add(c); };
        loopi(msize) { uchar c = ws.messages[i]; ws.messages.add(c); };
        ws.uses = 0;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            ENetPacket *packet;
            if(psize && (pkt[i].posoff<0 || psize-ci.position.length()>0))
            {
                packet = enet_packet_create(&ws.positions[pkt[i].posoff<0 ? 0 : pkt[i].posoff+ci.position.length()], 
                                            pkt[i].posoff<0 ? psize : psize-ci.position.length(), 
                                            ENET_PACKET_FLAG_NO_ALLOCATE);
                sendpacket(ci.clientnum, 0, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
                else { ++ws.uses; packet->freeCallback = freecallback; };
            };
            ci.position.setsizenodelete(0);

            if(msize && (pkt[i].msgoff<0 || msize-pkt[i].msglen>0))
            {
                packet = enet_packet_create(&ws.messages[pkt[i].msgoff<0 ? 0 : pkt[i].msgoff+pkt[i].msglen], 
                                            pkt[i].msgoff<0 ? msize : msize-pkt[i].msglen, 
                                            (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
                sendpacket(ci.clientnum, 1, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
                else { ++ws.uses; packet->freeCallback = freecallback; };
            };
            ci.messages.setsizenodelete(0);
        };
        reliablemessages = false;
        if(!ws.uses) 
        {
            delete &ws;
            return false;
        }
        else 
        {
            worldstates.add(&ws); 
            return true;
        };
    };

    bool sendpackets()
    {
        if(clients.empty()) return false;
        enet_uint32 curtime = enet_time_get()-lastsend;
        if(curtime<33) return false;
        bool flush = buildworldstate();
        lastsend += curtime - (curtime%33);
        return flush;
    };

    void parsepacket(int sender, int chan, bool reliable, ucharbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender<0) return;
        if(chan==2)
        {
            receivefile(sender, p.buf, p.maxlen);
            return;
        };
        if(reliable) reliablemessages = true;
        char text[MAXTRANS];
        int cn = -1, type;
        clientinfo *ci = sender>=0 ? (clientinfo *)getinfo(sender) : NULL;
#define QUEUE_MSG { if(!ci->local) while(curmsg<p.length()) ci->messages.add(p.buf[curmsg++]); }
        int curmsg;
        while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), ci))
        {
            case SV_POS:
            {
                cn = getint(p);
                if(cn<0 || cn>=getnbluelients() || cn!=sender)
                {
                    disconnect_client(sender, DISC_CN);
                    return;
                };
                vec oldpos(ci->o), newpos;
                loopi(3) newpos.v[i] = getuint(p)/DMF;
                if(!notgotitems && !notgotbases) ci->o = newpos;
                getuint(p);
                loopi(5) getint(p);
                int physstate = getint(p);
                if(physstate&0x20) loopi(2) getint(p);
                if(physstate&0x10) getint(p);
                int state = (getint(p)>>4) & 0x7;
                if(ci->spectator && state!=CS_SPECTATOR) break;
                if(m_capture)
                {
                    if(ci->state==CS_ALIVE)
                    {
                        if(state==CS_ALIVE) cps.movebases(ci->team, oldpos, ci->o);
                        else cps.leavebases(ci->team, oldpos);
                    }
                    else if(state==CS_ALIVE) cps.enterbases(ci->team, ci->o);
                };
                if(!notgotitems && !notgotbases) ci->state = state;
                if(!ci->local)
                {
                    ci->position.setsizenodelete(0);
                    while(curmsg<p.length()) ci->position.add(p.buf[curmsg++]);
                };
                break;
            };

            case SV_TEXT:
                getstring(text, p);
                QUEUE_MSG;
                break;

            case SV_INITC2S:
            {
                bool connected = !ci->name[0];
                getstring(text, p);
                s_strncpy(ci->name, text[0] ? text : "unnamed", MAXNAMELEN+1);
                if(!ci->local && connected)
                {
                    clientscore &sc = findscore(ci, false);
                    if(&sc) 
                    {
                        ci->score = sc;
                        sendf(-1, 1, "ri4", SV_RESUME, sender, sc.maxhealth, sc.frags);
                    };
                };
                QUEUE_MSG;
                curmsg = p.length();
                getstring(text, p);
                if(!ci->local && connected && m_teammode)
                {
                    const char *worst = chooseworstteam(text);
                    if(worst)
                    {
                        s_strcpy(text, worst);
                        sendf(sender, 1, "riis", SV_SETTEAM, sender, worst);
                        ucharbuf buf = ci->messages.reserve(2*strlen(worst)+1);
                        sendstring(worst, buf);
                        ci->messages.addbuf(buf);
                        curmsg = p.length();
                    };
                };
                if(m_capture && ci->state==CS_ALIVE && strcmp(ci->team, text)) cps.changeteam(ci->team, text, ci->o);
                s_strncpy(ci->team, text, MAXTEAMLEN+1);
                getint(p);
                QUEUE_MSG;
                break;
            };

            case SV_MAPVOTE:
            case SV_MAPCHANGE:
            {
                getstring(text, p);
                int reqmode = getint(p);
                if(type!=SV_MAPVOTE && !mapreload) break;
                if(!ci->local && !m_mp(reqmode)) reqmode = 0;
                vote(text, reqmode, sender);
                break;
            };

            case SV_ITEMLIST:
            {
                int n;
                while((n = getint(p))!=-1)
                {
                    server_entity se = { getint(p), false, 0 };
                    if(notgotitems)
                    {
                        while(sents.length()<=n) sents.add(se);
                        if(gamemode>=0 && (sents[n].type==I_QUAD)) sents[n].spawnsecs = rnd(60)+20;
                        else sents[n].spawned = true;
                    };
                };
                notgotitems = false;
                break;
            };

            case SV_TEAMSCORE:
                getstring(text, p);
                getint(p);
                QUEUE_MSG;
                break;

            case SV_BASEINFO:
                getint(p);
                getstring(text, p);
                getstring(text, p);
                getint(p);
                QUEUE_MSG;
                break;

            case SV_BASES:
            {
                int x;
                while((x = getint(p))!=-1)
                {
                    vec o;
                    o.x = x/DMF;
                    o.y = getint(p)/DMF;
                    o.z = getint(p)/DMF;
                    if(notgotbases) cps.addbase(o);
                };
                notgotbases = false;
                break;
            };

            case SV_ITEMPICKUP:
            {
                int n = getint(p);
                pickup(n, getint(p), sender);
                QUEUE_MSG;
                break;
            };

            case SV_PING:
                sendf(sender, 1, "i2", SV_PONG, getint(p));
                break;

            case SV_FRAGS:
            {
                int frags = getint(p);    
                if(minremain>=0) ci->score.frags = frags;
                QUEUE_MSG;
                break;
            };
                
            case SV_MASTERMODE:
            {
                int mm = getint(p);
                if(ci->master && mm>=MM_OPEN && mm<=MM_PRIVATE)
                {
                    mastermode = mm;
                    s_sprintfd(s)("mastermode is now %d", mastermode);
                    sendservmsg(s);
                };
                break;
            };
            
            case SV_KICK:
            {
                int victim = getint(p);
                if(ci->master && victim>=0 && victim<getnbluelients() && ci->clientnum!=victim && getinfo(victim))
                {
                    ban &b = bannedips.add();
                    b.time = lastsec;
                    b.ip = getclientip(victim);
                    disconnect_client(victim, DISC_KICK);
                };
                break;
            };

            case SV_SPECTATOR:
            {
                int spectator = getint(p), val = getint(p);
                if(!ci->master && spectator!=sender) break;
                if(spectator<0 || spectator>=getnbluelients()) break;
                clientinfo *spinfo = (clientinfo *)getinfo(spectator);
                if(!spinfo) break;
                if(!spinfo->spectator && val)
                {
                    if(m_capture && spinfo->state==CS_ALIVE) cps.leavebases(spinfo->team, spinfo->o);
                    spinfo->state = CS_SPECTATOR;
                };
                spinfo->spectator = val!=0;
                sendf(sender, 1, "ri3", SV_SPECTATOR, spectator, val);
                QUEUE_MSG;
                break;
            };

            case SV_SETTEAM:
            {
                int who = getint(p);
                getstring(text, p);
                if(!ci->master || who<0 || who>=getnbluelients()) break;
                clientinfo *wi = (clientinfo *)getinfo(who);
                if(!wi) break;
                if(m_capture && wi->state==CS_ALIVE && strcmp(wi->team, text)) cps.changeteam(wi->team, text, wi->o);
                s_strncpy(wi->team, text, MAXTEAMLEN+1);
                sendf(sender, 1, "riis", SV_SETTEAM, who, text);
                QUEUE_MSG;
                break;
            }; 

            case SV_FORCEINTERMISSION:
                if(m_sp) startintermission();
                break;

            case SV_GETMAP:
                if(mapdata)
                {
                    sendf(sender, 1, "ris", SV_SERVMSG, "server sending map...");
                    sendfile(sender, 2, mapdata);
                }
                else sendf(sender, 1, "ris", SV_SERVMSG, "no map to send"); 
                break;

            case SV_NEWMAP:
            {
                int size = getint(p);
                if(size>=0)
                {
                    smapname[0] = '\0';
                    resetitems();
                    notgotitems = notgotbases = false;
                };
                QUEUE_MSG;
                break;
            };

            case SV_SETMASTER:
            {
                int val = getint(p);
                getstring(text, p);
                setmaster(ci, val!=0, text);
                // don't broadcast the master password
                break;
            };

            default:
            {
                int size = msgsizelookup(type);
                if(size==-1) { disconnect_client(sender, DISC_TAGT); return; };
                loopi(size-1) getint(p);
                if(ci) QUEUE_MSG;
                break;
            };
        };
    };

    int welcomepacket(ucharbuf &p, int n)
    {
        int hasmap = (gamemode==1 && clients.length()) || smapname[0];
        putint(p, SV_INITS2C);
        putint(p, n);
        putint(p, PROTOCOL_VERSION);
        putint(p, hasmap);
        if(hasmap)
        {
            putint(p, SV_MAPCHANGE);
            sendstring(smapname, p);
            putint(p, gamemode);
            putint(p, SV_ITEMLIST);
            loopv(sents) if(sents[i].spawned)
            {
                putint(p, i);
                putint(p, sents[i].type);
            };
            putint(p, -1);
        };
        if(((clientinfo *)getinfo(n))->spectator)
        {
            putint(p, SV_SPECTATOR);
            putint(p, n);
            putint(p, 1);
        };
        loopv(clients)
        {
           clientinfo *ci = clients[i];
           if(ci->clientnum==n) continue;
           if(ci->score.maxhealth==100 && !ci->score.frags) continue;
           putint(p, SV_RESUME);
           putint(p, ci->clientnum);
           putint(p, ci->score.maxhealth);
           putint(p, ci->score.frags);
        };
        if(m_capture) cps.initclient(p);
        return 1;
    };

    void checkintermission()
    {
        if(!minremain)
        {
            interm = lastsec+10;
            mapend = lastsec+1000;
        };
        if(minremain>=0)
        {
            do minremain--; while(lastsec>mapend-minremain*60);
            sendf(-1, 1, "ri2", SV_TIMEUP, minremain+1);
        };
    };

    void startintermission() { minremain = 0; checkintermission(); };

    void serverupdate(int seconds)
    {
        loopv(sents)        // spawn entities when timer reached
        {
            if(sents[i].spawnsecs)
            {
                sents[i].spawnsecs -= seconds-lastsec;
                if(sents[i].spawnsecs<=0)
                {
                    sents[i].spawnsecs = 0;
                    sents[i].spawned = true;
                    sendf(-1, 1, "ri2", SV_ITEMSPAWN, i);
                }
                else if(sents[i].spawnsecs==10 && seconds-lastsec && (sents[i].type==I_QUAD))
                {
                    sendf(-1, 1, "ri2", SV_ANNOUNCE, sents[i].type);
                };
            };
        };
        
        if(m_capture) cps.updatescores(seconds);

        lastsec = seconds;
        
        while(bannedips.length() && bannedips[0].time+4*60*60<lastsec) bannedips.remove(0);
        
        if(masterupdate) { sendf(-1, 1, "ri2", SV_CURRENTMASTER, currentmaster); masterupdate = false; }; 
    
        if((gamemode>1 || (gamemode==0 && hasnonlocalclients())) && seconds>mapend-minremain*60) checkintermission();
        if(interm && seconds>interm)
        {
            interm = 0;
            checkvotes(true);
        };
    };

    void serverinit(char *sdesc, char *adminpass)
    {
        s_strcpy(serverdesc, sdesc);
        s_strcpy(masterpass, adminpass ? adminpass : "");
        smapname[0] = 0;
        resetitems();
    };
    
    void setmaster(clientinfo *ci, bool val, const char *pass = "")
    {
        if(val) 
        {
            loopv(clients) if(clients[i]->master)
            {
                if(masterpass[0] && !strcmp(masterpass, pass)) clients[i]->master = false;
                else return;
            };
        }        
        else if(!ci->master) return;
        ci->master = val;
        mastermode = MM_OPEN;
        currentmaster = val ? ci->clientnum : -1;
        masterupdate = true;
    };

    void localconnect(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        ci->local = true;
        clients.add(ci);
    };

    void localdisconnect(int n)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(m_capture && ci->state==CS_ALIVE) cps.leavebases(ci->team, ci->o);
        clients.removeobj(ci);
    };

    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = (clientinfo *)getinfo(n);
        ci->clientnum = n;
        clients.add(ci);
        loopv(bannedips) if(bannedips[i].ip==ip) return DISC_IPBAN;
        if(mastermode>=MM_PRIVATE) return DISC_PRIVATE;
        if(mastermode>=MM_LOCKED) 
        {
            ci->spectator = true;
            ci->state = CS_SPECTATOR;
        };
        if(currentmaster>=0) masterupdate = true;
        ci->gamestart = enet_time_get();
        return DISC_NONE;
    };

    void clientdisconnect(int n) 
    { 
        clientinfo *ci = (clientinfo *)getinfo(n);
        if(ci->master) setmaster(ci, false);
        if(m_capture && ci->state==CS_ALIVE) cps.leavebases(ci->team, ci->o);
        ci->score.timeplayed += enet_time_get() - ci->gamestart; 
        savescore(ci);
        sendf(-1, 1, "ri2", SV_CDIS, n); 
        clients.removeobj(ci);
        if(clients.empty()) bannedips.setsize(0); // bans clear when server empties
        else checkvotes();
    };

    char *servername() { return "Torment Server Alpha"; };
    int serverinfoport() { return SAUERBRATEN_SERVINFO_PORT; };
    int serverport() { return SAUERBRATEN_SERVER_PORT; };
	char *getdefaultmaster() { return "http://is.kicks-ass.org/"; }; 

    void serverinforeply(ucharbuf &p)
    {
        putint(p, clients.length());
        putint(p, 5);                   // number of attrs following
        putint(p, PROTOCOL_VERSION);    // a // generic attributes, passed back below
        putint(p, gamemode);            // b
        putint(p, minremain);           // c
        putint(p, maxclients);
        putint(p, mastermode);
        sendstring(smapname, p);
        sendstring(serverdesc, p);
    };

    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np)
    {
        return attr.length() && attr[0]==PROTOCOL_VERSION;
    };

    void serverinfostr(char *buf, const char *name, const char *sdesc, const char *map, int ping, const vector<int> &attr, int np)
    {
        if(attr[0]!=PROTOCOL_VERSION) s_sprintf(buf)("[%s protocol] %s", attr[0]<PROTOCOL_VERSION ? "older" : "newer", name);
        else 
        {
            string nbluel;
            if(attr.length()>=4) s_sprintf(nbluel)("%d/%d", np, attr[3]);
            else s_sprintf(nbluel)("%d", np);
            if(attr.length()>=5) switch(attr[4])
            {
                case MM_LOCKED: s_strcat(nbluel, " L"); break;
                case MM_PRIVATE: s_strcat(nbluel, " P"); break;
            };
            
            s_sprintf(buf)("%d\t%s\t%s, %s: %s %s", ping, nbluel, map[0] ? map : "[unknown]", modestr(attr[1]), name, sdesc);
        };
    };

    void receivefile(int sender, uchar *data, int len)
    {
        if(gamemode != 1 || len > 1024*1024) return;
        clientinfo *ci = (clientinfo *)getinfo(sender);
        if(ci->spectator && !ci->master) return;
        if(mapdata) { fclose(mapdata); mapdata = NULL; };
        if(!len) return;
        mapdata = tmpfile();
        if(!mapdata) return;
        fwrite(data, 1, len, mapdata);
        sendservmsg("[map uploaded to server, \"/getmap\" to receive it]");
    };
};
