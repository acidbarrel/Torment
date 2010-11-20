
struct entities : icliententities
{
    fpsclient &cl;
    
    vector<extentity *> ents;

    int triggertime;

    struct itemstat { int add, max, sound; char *name; } *itemstats;

    entities(fpsclient &_cl) : cl(_cl), triggertime(1)
    {
        static itemstat _itemstats[] =
        {
            {10,    30,    S_ITEMAMMO,   "SG"},
            {20,    60,    S_ITEMAMMO,   "MG"},
            {5,     15,    S_ITEMAMMO,   "RL"},
            {5,     15,    S_ITEMAMMO,   "RI"},
            {50,    30,    S_ITEMAMMO,   "PR"},
            {30,    120,   S_ITEMAMMO,   "PI"},
            {25,    100,   S_ITEMHEALTH, "H"},
            {10,    1000,  S_ITEMHEALTH, "MH"},
            {100,   100,   S_ITEMARMOUR, "GA"},
            {200,   200,   S_ITEMARMOUR, "YA"},
            {20000, 30000, S_ITEMPUP,    "Q"},
        };
        itemstats = _itemstats;
    };

    vector<extentity *> &getents() { return ents; };
    
    char *itemname(int i)
    {
        int t = ents[i]->type;
        if(t<I_SHELLS || t>I_QUAD) return NULL;
        return itemstats[t-I_SHELLS].name;
    };
    
    void renderent(extentity &e, const char *mdlname, float z, float yaw, int frame = 0, int anim = ANIM_MAPMODEL|ANIM_LOOP, int basetime = 0, float speed = 10.0f)
    {
        rendermodel(e.color, e.dir, mdlname, anim, 0, 0, e.o.x, e.o.y, z+e.o.z, yaw, 0, speed, basetime);
    };

    void renderentities()
    {
        if(cl.lastmillis>triggertime+1000) triggertime = 0;
        loopv(ents)
        {
            static char *entmdlnames[] =
            {
                "shells", "bullets", "rockets", "rrounds", "grenades", "cartridges", "health", "11pr",
                "g_armour", "y_armour", "b_armour", "quad", "teleporter", 
            };

            extentity &e = *ents[i];
            if(e.type==CARROT || e.type==RESPAWNPOINT)
            {
                renderent(e, e.type==CARROT ? "carrot" : "checkpoint", (float)(1+sin(cl.lastmillis/100.0+e.o.x+e.o.y)/20), cl.lastmillis/(e.attr2 ? 1.0f : 10.0f));
                continue;
            };
            if(!e.spawned && e.type!=TELEPORT) continue;
            if(e.type<I_SHELLS || e.type>TELEPORT) continue;
            renderent(e, entmdlnames[e.type-I_SHELLS], (float)(1+sin(cl.lastmillis/100.0+e.o.x+e.o.y)/20), cl.lastmillis/10.0f);
        };
    };

    void rumble(const extentity &e)
    {
        playsound(S_RUMBLE, &e.o);
    };

    void trigger(extentity &e)
    {
        if(e.attr3==29) cl.ms.endsp(false);
    };

    void baseammo(int gun) { cl.player1->ammo[gun] = itemstats[gun-1].add*2; };
    void repammo(int gun) 
    { 
        int &ammo = cl.player1->ammo[gun];
        ammo = max(ammo, itemstats[gun-1].add*2);
    };

    // these two functions are called when the server acknowledges that you really
    // picked up the item (in multiplayer someone may grab it before you).

    void radditem(int i, int &v)
    {
        itemstat &is = itemstats[ents[i]->type-I_SHELLS];
        ents[i]->spawned = false;
        v += is.add;
        if(v>is.max) v = is.max;
        cl.playsoundc(is.sound);
    };

    void realpickup(int n, fpsent *d)
    {
        if(isthirdperson())
        {
            char *name = itemname(n);
            if(name) particle_text(d->abovehead(), name, 15);
        };
        switch(ents[n]->type)
        {
        //  case I_PR:    radditem(n, d->ammo[GUN_PR]); break;
			case I_SHELLS:     radditem(n, d->ammo[GUN_SG]); break;
            case I_BULLETS:    radditem(n, d->ammo[GUN_CG]); break;
            case I_ROCKETS:    radditem(n, d->ammo[GUN_RL]); break;
            case I_ROUNDS:     radditem(n, d->ammo[GUN_RIFLE]); break;
            case I_GRENADES:   radditem(n, d->ammo[GUN_PR]); break;
            case I_CARTRIDGES: radditem(n, d->ammo[GUN_PISTOL]); break;
            case I_HEALTH:     radditem(n, d->health); break;
		    

            case pr:
               // d->maxhealth += 10;
               // conoutf("\f2you have a permanent +10 health bonus! (%d)", d->maxhealth);
               // playsound(S_V_BOOST);
               // radditem(n, d->health);
				d->ammo[GUN_PR] += 30;
                break;

            case I_GREENARMOUR:
                radditem(n, d->armour);
                d->armourtype = A_GREEN;
                break;

            case I_YELLOWARMOUR:
                radditem(n, d->armour);
                d->armourtype = A_YELLOW;
                break;

			

            case I_QUAD:
                radditem(n, d->quadmillis);
                conoutf("\f2you got the quad!");
                playsound(S_V_QUAD);
                break;
        };
    };

    // these functions are called when the client touches the item

    void additem(int i, int &v, int spawnsec)
    {
        if(v<itemstats[ents[i]->type-I_SHELLS].max)                              // don't pick up if not needed
        {
            int gamemode = cl.gamemode;
            cl.cc.addmsg(SV_ITEMPICKUP, "rii", i, m_classicsp ? 100000 : spawnsec);     // first ask the server for an ack
            ents[i]->spawned = false;                                            // even if someone else gets it first
        };
    };

    void teleport(int n, fpsent *d)     // also used by monsters
    {
        int e = -1, tag = ents[n]->attr1, beenhere = -1;
        for(;;)
        {
            e = findentity(TELEDEST, e+1);
            if(e==beenhere || e<0) { conoutf("no teleport destination for tag %d", tag); return; };
            if(beenhere<0) beenhere = e;
            if(ents[e]->attr2==tag)
            {
                d->o = ents[e]->o;
                d->yaw = ents[e]->attr1;
                d->pitch = 0;
                d->vel = vec(0, 0, 0);//vec(cosf(RAD*(d->yaw-90)), sinf(RAD*(d->yaw-90)), 0);
                entinmap(d);
                cl.playsoundc(S_TELEPORT);
                break;
            };
        };
    };

    void pickup(int n, fpsent *d)
    {
        int np = 1;
        loopv(cl.players) if(cl.players[i] && cl.players[i]->state!=CS_SPECTATOR) np++;
        np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
        int ammo = np*4;
        switch(ents[n]->type)
        {
            //case I_PR:         additem(n, d->ammo[GUN_PR], ammo); break;
			case I_SHELLS:     additem(n, d->ammo[GUN_SG], ammo); break;
            case I_BULLETS:    additem(n, d->ammo[GUN_CG], ammo); break;
            case I_ROCKETS:    additem(n, d->ammo[GUN_RL], ammo); break;
            case I_ROUNDS:     additem(n, d->ammo[GUN_RIFLE], ammo); break;
            case I_GRENADES:   additem(n, d->ammo[GUN_PR], ammo); break;
            case I_CARTRIDGES: additem(n, d->ammo[GUN_PISTOL], ammo); break;
            case I_HEALTH:     additem(n, d->health,  np*5); break;
            case pr:      additem(n, d->ammo[GUN_PR],  ammo); break;
		    

            case I_GREENARMOUR:
                // (100h/100g only absorbs 200 damage)
                if(d->armourtype==A_YELLOW && d->armour>=100) break;
                additem(n, d->armour, 20);
                break;

            case I_YELLOWARMOUR:
                additem(n, d->armour, 20);
                break;

            case I_QUAD:
                additem(n, d->quadmillis, 60);
                break;
                
            case TELEPORT:
            {
                static int lastteleport = 0;
                if(cl.lastmillis-lastteleport<500) break;
                lastteleport = cl.lastmillis;
                teleport(n, d);
                break;
            };

            case RESPAWNPOINT:
                if(n==cl.respawnent) break;
                cl.respawnent = n;
                conoutf("\f2respawn point set!");
                playsound(S_V_RESPAWNPOINT);
                break;

            case JUMPPAD:
            {
                static int lastjumppad = 0;
                if(cl.lastmillis-lastjumppad<300) break;
                lastjumppad = cl.lastmillis;
                vec v((int)(char)ents[n]->attr3*10.0f, (int)(char)ents[n]->attr2*10.0f, ents[n]->attr1*12.5f);
                cl.player1->timeinair = 0;
                cl.player1->gravity = vec(0, 0, 0);
                cl.player1->vel = v;
//                cl.player1->vel.z = 0;
//                cl.player1->vel.add(v);
                cl.playsoundc(S_JUMPPAD);
                break;
            };
        };
    };

    void checkitems()
    {
        if(editmode || cl.cc.spectator) return;
        itemstats[I_HEALTH-I_SHELLS].max = cl.player1->maxhealth;
        vec o = cl.player1->o;
        o.z -= cl.player1->eyeheight;
        loopv(ents)
        {
            extentity &e = *ents[i];
            if(e.type==NOTUSED) continue;
            if(!e.spawned && e.type!=TELEPORT && e.type!=JUMPPAD && e.type!=RESPAWNPOINT) continue;
            float dist = e.o.dist(o);
            if(dist<(e.type==TELEPORT ? 16 : 12)) pickup(i, cl.player1);
        };
    };

    void checkquad(int time)
    {
        if(cl.player1->quadmillis && (cl.player1->quadmillis -= time)<0)
        {
            cl.player1->quadmillis = 0;
            cl.playsoundc(S_PUPOUT);
            conoutf("\f2quad damage is over");
        };
    };

    void putitems(ucharbuf &p, int gamemode)            // puts items in network stream and also spawns them locally
    {
        loopv(ents) if(ents[i]->type>=I_SHELLS && ents[i]->type<=I_QUAD && (!m_capture || ents[i]->type<I_SHELLS || ents[i]->type>I_CARTRIDGES))
        {
            putint(p, i);
            putint(p, ents[i]->type);
            
            ents[i]->spawned = (m_sp || (ents[i]->type!=I_QUAD && ents[i]->type!=pr)); 
        };
    };

    void resetspawns() { loopv(ents) ents[i]->spawned = false; };
    void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i]->spawned = on; };

    extentity *newentity() { return new fpsentity(); };

    void fixentity(extentity &e)
    {
        switch(e.type)
        {
            case MONSTER:
            case TELEDEST:
                e.attr2 = e.attr1;
            case RESPAWNPOINT:
                e.attr1 = (int)cl.player1->yaw;
        };
    };

    const char *entnameinfo(entity &e) { return ""; };
    const char *entname(int i)
    {
        static const char *entnames[] =
        {
            "none?", "light", "mapmodel", "playerstart", "envmap",
            "shells", "bullets", "rockets", "riflerounds", "grenades", "cartridges",
            "health", "pr", "greenarmour", "yellowarmour", "quaddamage",
            "teleport",  "teledest", 
            "monster", "carrot", "jumppad",
            "base", "respawnpoint"
            "", "", "", "",
			
        };
        return i>=0 && size_t(i)<sizeof(entnames)/sizeof(entnames[0]) ? entnames[i] : "";
    };
    
    int extraentinfosize() { return 0; };       // size in bytes of what the 2 methods below read/write... so it can be skipped by other games

    void writeent(entity &e, char *buf)   // write any additional data to disk (except for ET_ ents)
    {
    };

    void readent(entity &e, char *buf)     // read from disk, and init
    {
        int ver = getmapversion();
        if(ver <= 10)
        {
            if(e.type >= 7) e.type++;
        };
        if(ver <= 12)
        {
            if(e.type >= 8) e.type++;
        };
    };

    void editent(int i)
    {
        extentity &e = *ents[i];
        if(e.type==BASE)
        {
            int gamemode = cl.gamemode;
            if(m_capture) cl.cpc.setupbases();
        };
        cl.cc.addmsg(SV_EDITENT, "ri9", i, (int)(e.o.x*DMF), (int)(e.o.y*DMF), (int)(e.o.z*DMF), e.type, e.attr1, e.attr2, e.attr3, e.attr4);
    };

    float dropheight(entity &e)
    {
        if(e.type==MAPMODEL || e.type==BASE) return 0.0f;
        return 4.0f;
    };
};
