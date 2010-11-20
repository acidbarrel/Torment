
// network quantization scale
#define DMF 16.0f                // for world locations
#define DNF 100.0f              // for normalized vectors
#define DVELF 1.0f              // for playerspeed based velocity vectors

enum                            // static entity types
{
    NOTUSED = ET_EMPTY,         // entity slot not in use in map
    LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
    MAPMODEL = ET_MAPMODEL,     // attr1 = angle, attr2 = idx
    PLAYERSTART,                // attr1 = angle
    ENVMAP = ET_ENVMAP,         // attr1 = radius
    I_SHELLS, I_BULLETS, I_ROCKETS, I_ROUNDS, I_GRENADES, I_CARTRIDGES,
    I_HEALTH, pr,
    I_GREENARMOUR, I_YELLOWARMOUR,
    I_QUAD,
    TELEPORT,                   // attr1 = idx
    TELEDEST,                   // attr1 = angle, attr2 = idx
    MONSTER,                    // attr1 = angle, attr2 = monstertype
    CARROT,                     // attr1 = tag, attr2 = type
    JUMPPAD,                    // attr1 = zpush, attr2 = ypush, attr3 = xpush
    BASE,
    RESPAWNPOINT,
    MAXENTTYPES,
	
};

struct fpsentity : extentity
{
    // extend with additional fields if needed...
};

enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_RIFLE, GUN_GL, GUN_PISTOL, GUN_FIREBALL, GUN_PR, GUN_SLIMEBALL, GUN_BITE, NUMGUNS };
enum { A_BLUE, A_GREEN, A_YELLOW};     // armour types... take 20/40/60 % off //ADDED BLUE 
enum { M_NONE = 0, M_SEARCH, M_HOME, M_ATTACKING, M_PAIN, M_SLEEP, M_AIMING };  // monster states

struct fpsent : dynent
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int health, armour, armourtype, quadmillis;
    int maxhealth;
    int lastpain;
    int gunselect, gunwait;
    int lastaction, lastattackgun;
    bool attacking;
    int ammo[NUMGUNS];
    int superdamage;
    int frags, deaths, totaldamage, totalshots;
    editinfo *edit;

    string name, team, info;

    fpsent() : weight(100), clientnum(-1), lastupdate(0), plag(0), ping(0), lifesequence(0), maxhealth(100), lastpain(0), frags(0), deaths(0), totaldamage(0), totalshots(0), edit(NULL)
               { name[0] = team[0] = info[0] = 0; respawn(); };
    ~fpsent() { freeeditinfo(edit); };

    void respawn()
    {
        reset();
        health = maxhealth;
        armour = 0;
        armourtype = A_BLUE;
        quadmillis = gunwait = lastaction = 0;
        lastattackgun = gunselect = GUN_PISTOL;
        attacking = false;
        loopi(NUMGUNS) ammo[i] = 0;
        ammo[GUN_FIST] = 1;
        superdamage = 0;
    };
};

extern int gamemode, nextmode;
extern vector<fpsent *> players;                 // all the other clients (in multiplayer)
extern fpsent *player1;                 // special client ent that receives input

#define m_noitems     (gamemode>=4 && gamemode<12)
#define m_noitemsrail ((gamemode>=4 && gamemode<=5) || (gamemode>=8 && gamemode<=9))
#define m_arena       (gamemode>=8 && gamemode<12)
#define m_ra      (gamemode>=10 && gamemode<12)
#define m_capture     (gamemode==12)
#define m_teammode    ((gamemode&1 && gamemode>2) || m_capture)
#define m_sp          (gamemode<0)
#define m_dmsp        (gamemode==-1)
#define m_classicsp   (gamemode==-2)
#define isteam(a,b)   (m_teammode && strcmp(a, b)==0)

#define m_mp(mode)       (mode>=0 && mode<=12)

// hardcoded sounds, defined in sounds.cfg
enum
{
    S_JUMP = 0, S_LAND, S_RIFLE, S_PUNCH1, S_SG, S_CG,
    S_RLFIRE, S_RLHIT, S_WEAPLOAD, S_ITEMAMMO, S_ITEMHEALTH,
    S_ITEMARMOUR, S_ITEMPUP, S_ITEMSPAWN, S_TELEPORT, S_NOAMMO, S_PUPOUT,
    S_PAIN1, S_PAIN2, S_PAIN3, S_PAIN4, S_PAIN5, S_PAIN6,
    S_DIE1, S_DIE2,
    S_FLAUNCH, S_FEXPLODE,
    S_SPLASH1, S_SPLASH2,
    S_GRUNT1, S_GRUNT2, S_RUMBLE,
    S_PAINO,
    S_PAINR, S_DEATHR,
    S_PAINE, S_DEATHE,
    S_PAINS, S_DEATHS,
    S_PAINB, S_DEATHB,
    S_PAINP, S_PIGGR2,
    S_PAINH, S_DEATHH,
    S_PAIND, S_DEATHD,
    S_PIGR1, S_ICEBALL, S_SLIMEBALL,
    S_JUMPPAD, S_PISTOL,
    
    S_V_BASECAP, S_V_BASELOST,
    S_V_FIGHT,
    S_V_BOOST, S_V_BOOST10,
    S_V_QUAD, S_V_QUAD10,
    S_V_RESPAWNPOINT, 
	
	S_TALK,
	S_TAUNT1,
	S_TAUNT2,
	S_TAUNT3
};


// network messages codes, c2s, c2c, s2c
enum
{
    SV_INITS2C = 0, SV_INITC2S, SV_POS, SV_TEXT, SV_SOUND, SV_CDIS,
    SV_DIED, SV_DAMAGE, SV_SHOT, SV_FRAGS, SV_GUNSELECT,
    SV_MAPCHANGE, SV_MAPVOTE, SV_ITEMSPAWN, SV_ITEMPICKUP, SV_DENIED,
    SV_PING, SV_PONG, SV_CLIENTPING,
    SV_TIMEUP, SV_MAPRELOAD, SV_ITEMACC,
    SV_SERVMSG, SV_ITEMLIST, SV_RESUME,
    SV_EDITENT, SV_EDITF, SV_EDITT, SV_EDITM, SV_FLIP, SV_COPY, SV_PASTE, SV_ROTATE, SV_REPLACE, SV_DELCUBE, SV_NEWMAP, SV_GETMAP,
    SV_MASTERMODE, SV_KICK, SV_CURRENTMASTER, SV_SPECTATOR, SV_SETMASTER, SV_SETTEAM,
    SV_BASES, SV_BASEINFO, SV_TEAMSCORE, SV_REPAMMO, SV_FORCEINTERMISSION, SV_ANNOUNCE,
    SV_CLIENT,
};

#define SAUERBRATEN_SERVER_PORT 10666
#define SAUERBRATEN_SERVINFO_PORT 10667
#define PROTOCOL_VERSION 006           // bump when protocol changes

#define MAXNAMELEN 15
#define MAXTEAMLEN 4
