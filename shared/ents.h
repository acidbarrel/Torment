// this file defines static map entities ("entity") and dynamic entities (players/monsters, "dynent")
// the gamecode extends these types to add game specific functionality

// ET_*: the only static entity types dictated by the engine... rest are gamecode dependent

enum { ET_EMPTY=0, ET_LIGHT, ET_MAPMODEL, ET_PLAYERSTART, ET_ENVMAP, ET_GAMESPECIFIC };

struct entity                                   // persistent map entity
{
    vec o;                                      // position
    short attr1, attr2, attr3, attr4, attr5;
    uchar type;                                 // type is one of the above
    uchar reserved;
};

enum
{
    TRIGGER_RESET = 0,
    TRIGGERING,
    TRIGGERED,
    TRIGGER_RESETTING,
    TRIGGER_DISAPPEARED
};

struct extentity : entity                       // part of the entity that doesn't get saved to disk
{
    uchar spawned, inoctanode, visible, triggerstate;        // the only dynamic state of a map entity
    vec color, dir;
    int lasttrigger;

    extentity() : visible(false), triggerstate(TRIGGER_RESET), lasttrigger(0) {};
};

//extern vector<extentity *> ents;                // map entities

struct animstate                                // used for animation blending of animated characters
{
    int anim, frame, range, basetime;
    float speed;
    animstate() : anim(0), frame(0), range(0), basetime(0), speed(100.0f) { };

    bool operator==(const animstate &o) const { return frame==o.frame && range==o.range && basetime==o.basetime && speed==o.speed; };
    bool operator!=(const animstate &o) const { return frame!=o.frame || range!=o.range || basetime!=o.basetime || speed!=o.speed; };
};

enum { ANIM_DYING = 0, ANIM_DEAD, ANIM_PAIN, ANIM_IDLE, ANIM_IDLE_ATTACK, ANIM_RUN, ANIM_RUN_ATTACK, ANIM_EDIT, ANIM_LAG, ANIM_JUMP, ANIM_JUMP_ATTACK, ANIM_GUNSHOOT, ANIM_GUNIDLE, ANIM_MAPMODEL, ANIM_TRIGGER, NUMANIMS };

#define ANIM_INDEX      0xFF
#define ANIM_LOOP       (1<<8)
#define ANIM_START      (1<<9)
#define ANIM_END        (1<<10)
#define ANIM_REVERSE    (1<<11)

enum { CS_ALIVE = 0, CS_DEAD, CS_LAGGED, CS_EDITING, CS_SPECTATOR };

enum { PHYS_FLOAT = 0, PHYS_FALL, PHYS_SLIDE, PHYS_SLOPE, PHYS_FLOOR, PHYS_STEP_UP, PHYS_STEP_DOWN, PHYS_BOUNCE };

enum { ENT_PLAYER = 0, ENT_AI, ENT_CAMERA, ENT_BOUNCE };

struct physent                                  // base entity type, can be affected by physics
{
    vec o, vel, gravity;                        // origin, velocity, accumulated gravity
    float yaw, pitch, roll;
    float maxspeed;                             // cubes per second, 100 for player
    int timeinair;
    float radius, eyeheight, aboveeye;          // bounding box size
    vec floor;                                  // the normal of floor the dynent is on

    bool inwater;
    bool jumpnext;
    bool blocked, moving;                       // used by physics to signal ai

    char move, strafe;

    uchar physstate;                            // one of PHYS_* above
    uchar state;                                // one of CS_* above
    uchar type;                                 // one of ENT_* above

    physent() : o(0, 0, 0), yaw(270), pitch(0), roll(0), maxspeed(100), 
               radius(4.1f), eyeheight(14), aboveeye(1), 
               inwater(false), blocked(false), moving(true), state(CS_ALIVE), type(ENT_PLAYER)
               { reset(); };
               
    void reset()
    {
        timeinair = strafe = move = 0;
        physstate = PHYS_FALL;
        vel = gravity = vec(0, 0, 0);
    };
};

struct dynent : physent                         // animated characters, or characters that can receive input
{
    bool k_left, k_right, k_up, k_down;         // see input code

    animstate prev[2], current[2];              // md2's need only [0], md3's need both for the lower&upper model
    int lastanimswitchtime[2];
    void *lastmodel[2];
    
    dynent() { reset(); loopi(2) { lastanimswitchtime[i] = -1; lastmodel[i] = NULL; }; };
               
    void stopmoving()
    {
        k_left = k_right = k_up = k_down = jumpnext = false;
        move = strafe = 0;
    };
        
    void reset()
    {
        physent::reset();
        stopmoving();
    };

    vec abovehead() { return vec(o).add(vec(0, 0, aboveeye+4)); };
};


