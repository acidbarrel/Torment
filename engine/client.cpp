// client.cpp, mostly network related client game code

#include "pch.h"
#include "engine.h"

ENetHost *clienthost = NULL;
int connecting = 0;
int connattempts = 0;
int disconnecting = 0;

bool multiplayer(bool msg)
{
    // check not correct on listen server?
    if(clienthost && msg) conoutf("operation not available in multiplayer");
    return clienthost != NULL;
};

void setrate(int rate)
{
   if(!clienthost || connecting) return;
   enet_host_bandwidth_limit(clienthost, rate, rate);
};

VARF(rate, 0, 0, 25000, setrate(rate));

void throttle();

VARF(throttle_interval, 0, 5, 30, throttle());
VARF(throttle_accel,    0, 2, 32, throttle());
VARF(throttle_decel,    0, 2, 32, throttle());

void throttle()
{
    if(!clienthost || connecting) return;
    ASSERT(ENET_PEER_PACKET_THROTTLE_SCALE==32);
    enet_peer_throttle_configure(clienthost->peers, throttle_interval*1000, throttle_accel, throttle_decel);
};

void connects(char *servername)
{   
    disconnect(1);  // reset state

    ENetAddress address;
    address.port = sv->serverport();

    if(servername)
    {
        addserver(servername);
        conoutf("attempting to connect to %s", servername);
        if(!resolverwait(servername, &address))
        {
            conoutf("could not resolve server %s", servername);
            return;
        };
    }
    else
    {
        conoutf("attempting to connect over LAN");
        address.host = ENET_HOST_BROADCAST;
    };

    clienthost = enet_host_create(NULL, 1, rate, rate);

    if(clienthost)
    {
        enet_host_connect(clienthost, &address, cc->nbluehannels()); 
        enet_host_flush(clienthost);
        connecting = lastmillis;
        connattempts = 0;
    }
    else
    {
        conoutf("\f3could not connect to server");
        disconnect();
    };
};

void lanconnect()
{
    connects(0);
};

void disconnect(int onlyclean, int async)
{
    if(clienthost) 
    {
        if(!connecting && !disconnecting) 
        {
            enet_peer_disconnect(clienthost->peers, DISC_NONE);
            enet_host_flush(clienthost);
            disconnecting = lastmillis;
        };
        if(clienthost->peers->state != ENET_PEER_STATE_DISCONNECTED)
        {
            if(async) return;
            enet_peer_reset(clienthost->peers);
        };
        enet_host_destroy(clienthost);
    };

    if(clienthost && !connecting) conoutf("disconnected");
    clienthost = NULL;
    connecting = 0;
    connattempts = 0;
    disconnecting = 0;
    
    cc->gamedisconnect();
    
    localdisconnect();

    if(!onlyclean) { localconnect(); cc->gameconnect(false); };
};

void trydisconnect()
{
    if(!clienthost)
    {
        conoutf("not connected");
        return;
    };
    if(connecting) 
    {
        conoutf("aborting connection attempt");
        disconnect();
        return;
    };
    conoutf("attempting to disconnect...");
    disconnect(0, !disconnecting);
};

COMMANDN(connect, connects, "s");
COMMAND(lanconnect, "");
COMMANDN(disconnect, trydisconnect, "");

int lastupdate = 0;

bool netmapstart() { return clienthost!=NULL; };

void sendpackettoserv(ENetPacket *packet, int chan)
{
    if(clienthost) enet_host_broadcast(clienthost, chan, packet);
    else localclienttoserver(chan, packet);
};

void c2sinfo(dynent *d, int rate)                     // send update to the server
{
    if(lastmillis-lastupdate<rate) return;    // don't update faster than 30fps
    lastupdate = lastmillis;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, 0);
    ucharbuf p(packet->data, packet->dataLength);
    bool reliable = false;
    int chan = cc->sendpacketclient(p, reliable, d);
    if(!p.length()) { enet_packet_destroy(packet); return; };
    if(reliable) packet->flags = ENET_PACKET_FLAG_RELIABLE;
    enet_packet_resize(packet, p.length());
    sendpackettoserv(packet, chan);
    if(clienthost) enet_host_flush(clienthost);
};

void neterr(char *s)
{
    conoutf("\f3illegal network message (%s)", s);
    disconnect();
};

void localservertoclient(int chan, uchar *buf, int len)   // processes any updates from the server
{
    ucharbuf p(buf, len);
    cc->parsepacketclient(chan, p);
};

void clientkeepalive() { if(clienthost) enet_host_service(clienthost, NULL, 0); };

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost) return;
    if(connecting && lastmillis/3000 > connecting/3000)
    {
        conoutf("attempting to connect...");
        connecting = lastmillis;
        ++connattempts; 
        if(connattempts > 3)
        {
            conoutf("\f3could not connect to server");
            disconnect();
            return;
        };
    };
    while(clienthost!=NULL && enet_host_service(clienthost, &event, 0)>0)
    switch(event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            conoutf("connected to server");
            connecting = 0;
            throttle();
            if(rate) setrate(rate);
            cc->gameconnect(true);
            break;
         
        case ENET_EVENT_TYPE_RECEIVE:
            if(disconnecting) conoutf("attempting to disconnect...");
            else localservertoclient(event.channelID, event.packet->data, (int)event.packet->dataLength); break;
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            extern char *disc_reasons[];
            if(event.data>DISC_MAXCLIENTS) event.data = DISC_NONE;
            if(!disconnecting || event.data) conoutf("\f3server network error, disconnecting (%s) ...", disc_reasons[event.data]);
            disconnect();
            return;

        default:
            break;
    };
};

