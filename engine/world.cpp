// world.cpp: core map management stuff

#include "pch.h"
#include "engine.h"

header hdr;
vector<ushort> texmru;

VAR(octaentsize, 0, 128, 1024);
VAR(entselradius, 0, 2, 10);

void modifyoctaentity(bool add, int id, cube *c, const ivec &cor, int size, const ivec &bo, const ivec &br, vtxarray *lastva = NULL)
{
    loopoctabox(cor, size, bo, br)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        vtxarray *va = c[i].ext && c[i].ext->va ? c[i].ext->va : lastva;
        if(c[i].children != NULL && size > octaentsize)
            modifyoctaentity(add, id, c[i].children, o, size>>1, bo, br, va);
        else if(add)
        {
            if(!c[i].ext || !c[i].ext->ents) ext(c[i]).ents = new octaentities(o, size);
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    if(loadmodel(NULL, et->getents()[id]->attr2))
                    {
                        if(va && c[i].ext->ents->mapmodels.empty()) 
                        {
                            if(!va->mapmodels) va->mapmodels = new vector<octaentities *>;
                            va->mapmodels->add(c[i].ext->ents);
                        };
                        c[i].ext->ents->mapmodels.add(id);
                        break;
                    };
                    // invisible mapmodel
                default:
                    c[i].ext->ents->other.add(id);
                    break;
            };

        }
        else if(c[i].ext && c[i].ext->ents)
        {
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    if(loadmodel(NULL, et->getents()[id]->attr2))
                    {
                        c[i].ext->ents->mapmodels.removeobj(id);
                        if(va && va->mapmodels && c[i].ext->ents->mapmodels.empty())
                        {
                            va->mapmodels->removeobj(c[i].ext->ents);
                            if(va->mapmodels->empty()) DELETEP(va->mapmodels);
                        };
                        break;
                    };
                    // invisible mapmodel
                default:
                    c[i].ext->ents->other.removeobj(id);
                    break;
            };
            if(c[i].ext->ents->mapmodels.empty() && c[i].ext->ents->other.empty()) 
                freeoctaentities(c[i]);
        };
        if(c[i].ext && c[i].ext->ents) c[i].ext->ents->query = NULL;
    };
};

bool getentboundingbox(extentity &e, ivec &o, ivec &r)
{
    switch(e.type)
    {
        case ET_EMPTY:
            return false;
        case ET_MAPMODEL:
        {
            model *m = loadmodel(NULL, e.attr2);
            if(m)
            {
                vec center, radius;
                m->boundbox(0, center, radius);
                rotatebb(center, radius, e.attr1);
                
                o = e.o;
                o.add(center);
                r = radius;
                r.add(1);
                o.sub(r);
                r.mul(2);
                break;
            };
        };
        // invisible mapmodels use entselradius
        default:
            o = e.o;
            o.sub(entselradius);
            r.x = r.y = r.z = entselradius*2;
            break;
    };
    return true;
};

static void modifyoctaent(bool add, int id)
{
    ivec o, r;
    extentity &e = *et->getents()[id];
    if((e.inoctanode!=0)==add || !getentboundingbox(e, o, r)) return;
    e.inoctanode = add;
    modifyoctaentity(add, id, worldroot, ivec(0, 0, 0), hdr.worldsize>>1, o, r);
    if(e.type == ET_LIGHT) clearlightcache(id);
    else if(add) lightent(e);
};

static inline void addentity(int id)    { modifyoctaent(true,  id); };
static inline void removeentity(int id) { modifyoctaent(false, id); };

void freeoctaentities(cube &c)
{
    if(!c.ext) return;
    while(c.ext->ents && !c.ext->ents->mapmodels.empty()) removeentity(c.ext->ents->mapmodels.pop());
    while(c.ext->ents && !c.ext->ents->other.empty())     removeentity(c.ext->ents->other.pop());
    if(c.ext->ents)
    {
        delete c.ext->ents;
        c.ext->ents = NULL;
    };
};

void entitiesinoctanodes()
{
    loopv(et->getents()) addentity(i);
};

extern selinfo sel;
extern bool havesel, selectcorners;
extern bool undogoahead;
int efocus = -1;

int closestent()        // used for delent and edit mode ent display
{
    if(!editmode) return -1;
    int best = -1;
    float bdist = 99999;
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        entity &e = *ents[i];
        if(e.type==ET_EMPTY) continue;
        float dist = e.o.dist(player->o);
        if(dist<bdist)
        {
            best = i;
            bdist = dist;
        };
    };
    return efocus = (bdist==99999 ? -1 : best);
};

bool pointinsel(selinfo &sel, vec &o)
{
    return(o.x <= sel.o.x+sel.s.x*sel.grid
        && o.x >= sel.o.x
        && o.y <= sel.o.y+sel.s.y*sel.grid
        && o.y >= sel.o.y
        && o.z <= sel.o.z+sel.s.z*sel.grid
        && o.z >= sel.o.z);
};

vector<int> entgroup;

bool haveselent()
{
    return entgroup.length() > 0;
};

extern void entcancel();

void initundoent(undoblock &u)
{
    u.n = 0; u.e = NULL;
    u.n = entgroup.length();
    if(u.n<=0) return;
    u.e = new undoent[u.n];
    loopv(entgroup)
    {
        u.e->i = entgroup[i];
        u.e->e = *et->getents()[entgroup[i]];
        u.e++;
    };
    u.e -= u.n;    
};

void makeundoent()
{
    undoblock u;
    initundoent(u);
    if(u.n) addundo(u);
};

// convenience macros implicitly define:
// e         entity, currently edited ent
// n         int,    index to currently edited ent
#define implicitent(f)  { if(entgroup.empty()) { int _ = closestent(); if(_>=0) { entgroup.add(_); f; entcancel(); }; } else f; }
#define entfocus(i, f)  { int n = efocus = (i); if(n>=0) { entity &e = *et->getents()[n]; f; }; }
#define entedit(i, f)   { entfocus(i, removeentity(n); f; addentity(n); et->editent(n)); }
#define addgroup(exp)   { loopv(et->getents()) entfocus(i, if(exp) entgroup.add(n)); }
#define setgroup(exp)   { entcancel(); addgroup(exp); }
#define groupeditpure(f){ loopv(entgroup) entedit(entgroup[i], f); }
#define groupeditundo(f){ makeundoent(); groupeditpure(f); }
#define groupedit(f)    { implicitent(groupeditundo(f)); }

void copyundoents(undoblock &d, undoblock &s)
{
    entcancel();
    loopi(s.n)
        entgroup.add(s.e[i].i);
    initundoent(d);
};

void pasteundoents(undoblock &u)
{
    loopi(u.n)
        entedit(u.e[i].i, e = u.e[i].e);
};

void entflip()
{
    if(noedit(true)) return;
    int d = dimension(sel.orient);
    float mid = sel.s[d]*sel.grid/2+sel.o[d];
    groupeditundo(e.o[d] -= (e.o[d]-mid)*2);
};

void entrotate(int *cw)
{
    if(noedit(true)) return;
    int d = dimension(sel.orient);
    int dd = *cw<0 == dimcoord(sel.orient) ? R[d] : C[d];
    float mid = sel.s[dd]*sel.grid/2+sel.o[dd];
    vec s(sel.o.v);
    groupeditundo(
        e.o[dd] -= (e.o[dd]-mid)*2;
        e.o.sub(s);
        swap(float, e.o[R[d]], e.o[C[d]]);
        e.o.add(s);
    );
};

VAR(entselsnap, 0, 0, 1);

extern void editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first);

void entdrag(const vec &ray, int d, ivec &dest, bool first)
{
    if(!haveselent()) return;

    float r = 0, c = 0;
    static vec v, handle;

    entfocus(entgroup.last(),        
        editmoveplane(e.o, ray, d, e.o[D[d]], handle, v, first);
        dest = v;
        int z = dest[d]&(~(sel.grid-1));
        dest.add(sel.grid/2).mask(~(sel.grid-1));
        dest[d] = z;
        r = (entselsnap ? dest[R[d]] : v[R[d]]) - e.o[R[d]];
        c = (entselsnap ? dest[C[d]] : v[C[d]]) - e.o[C[d]];       
    );

    if(first) makeundoent();
    groupeditpure(e.o[R[d]] += r; e.o[C[d]] += c);   
};

bool enttoggle(int id)
{
    int i = entgroup.find(id);
    if(i < 0)
        entgroup.add(id);
    else
        entgroup.remove(i);
    return i < 0;
};

VARF(entmoving, 0, 0, 1,
    if(entmoving == 1)
    {
        vec ray(worldpos); 
        ray.sub(player->o);
        int id = rayent(player->o, ray);
        ivec t;
        if(id >= 0 && enttoggle(id))
            entdrag(ray, dimension(sel.orient), t, true);
        else 
            entmoving = 0;
    };
);

void entpush(int *dir)
{
    if(noedit(true)) return;
    int d = dimension(sel.orient);
    int s = dimcoord(sel.orient) ? -*dir : *dir;
    if(entmoving) 
    {
        groupeditpure(e.o[d] += float(s*sel.grid)); // editdrag supplies the undo
    }
    else 
        groupedit(e.o[d] += float(s*sel.grid));
};

COMMAND(entflip, "");
COMMAND(entrotate, "i");
COMMAND(entpush, "i");

void delent()
{
    if(noedit(true)) return;
    groupedit(
        e.type = ET_EMPTY;
        et->editent(n);
        continue;
    );
    entcancel();
};

int findtype(char *what)
{
    for(int i = 0; *et->entname(i); i++) if(strcmp(what, et->entname(i))==0) return i;
    conoutf("unknown entity type \"%s\"", what);
    return ET_EMPTY;
}

VAR(entdrop, 0, 2, 3);

bool dropentity(entity &e, int drop = -1)
{
    vec radius(4.0f, 4.0f, 4.0f);
    if(drop<0) drop = entdrop;
    if(e.type == ET_MAPMODEL)
    {
        model *m = loadmodel(NULL, e.attr2);
        if(m)
        {
            vec center;
            m->boundbox(0, center, radius);
            rotatebb(center, radius, e.attr1);
            radius.x += fabs(center.x);
            radius.y += fabs(center.y);
        };
        radius.z = 0.0f;
    };
    switch(drop)
    {
    case 1:
        if(e.type != ET_LIGHT)
            dropenttofloor(&e);
        break;
    case 2:
    case 3:
        int cx = 0, cy = 0;
        if(sel.cxs == 1 && sel.cys == 1)
        {
            cx = (sel.cx ? 1 : -1) * sel.grid / 2;
            cy = (sel.cy ? 1 : -1) * sel.grid / 2;
        }
        e.o = sel.o.v;
        int d = dimension(sel.orient), dc = dimcoord(sel.orient);
        e.o[R[d]] += sel.grid / 2 + cx;
        e.o[C[d]] += sel.grid / 2 + cy;
        if(!dc)
            e.o[D[d]] -= radius[D[d]];
        else
            e.o[D[d]] += sel.grid + radius[D[d]];

        if(drop == 3)
            dropenttofloor(&e);
        break;
    };
    return true;
};

void dropent()
{
    if(noedit(true)) return;
    groupedit(dropentity(e));
};

extentity *newentity(bool local, const vec &o, int type, int v1, int v2, int v3, int v4)
{
    extentity &e = *et->newentity();
    e.o = o;
    e.attr1 = v1;
    e.attr2 = v2;
    e.attr3 = v3;
    e.attr4 = v4;
    e.attr5 = 0;
    e.type = type;
    e.reserved = 0;
    e.spawned = false;
    e.inoctanode = false;
    e.color = vec(1, 1, 1);
    if(local)
    {
        switch(type)
        {
                case ET_MAPMODEL:
                    e.attr4 = e.attr3;
                    e.attr3 = e.attr2;
                    e.attr2 = e.attr1;
                case ET_PLAYERSTART:
                    e.attr1 = (int)camera1->yaw;
                    break;
        };
        et->fixentity(e);
    };
    return &e;
};

void newentity(int type, int a1, int a2, int a3, int a4)
{
    extentity *t = newentity(true, player->o, type, a1, a2, a3, a4);
    dropentity(*t);
    et->getents().add(t);
    int i = et->getents().length()-1;
    t->type = ET_EMPTY;
    enttoggle(i);
    makeundoent();
    entedit(i, e.type = type);
};

void newent(char *what, int *a1, int *a2, int *a3, int *a4)
{
    if(noedit(true)) return;
    int type = findtype(what);
    if(type != ET_EMPTY)
        newentity(type, *a1, *a2, *a3, *a4);
};

int entcopygrid;
vector<entity> entcopybuf;

void entcopy()
{
    entcopygrid = sel.grid;
    entcopybuf.setsize(0);
    loopv(entgroup) 
        entfocus(entgroup[i], entcopybuf.add(e).o.sub(sel.o.v));
};

void entpaste()
{
    if(entcopybuf.length()==0) return;
    entcancel();
    int last = et->getents().length()-1;
    float m = float(sel.grid)/float(entcopygrid);
    loopv(entcopybuf)
    {
        entity &c = entcopybuf[i];
        vec o(c.o);
        o.mul(m).add(sel.o.v);
        extentity *e = newentity(true, o, ET_EMPTY, c.attr1, c.attr2, c.attr3, c.attr4);
        et->getents().add(e);
        entgroup.add(++last);
    };
    int j = 0;
    groupeditundo(e.type = entcopybuf[j++].type);
};

COMMAND(newent, "siiii");
COMMAND(delent, "");
COMMAND(dropent, "");
COMMAND(entcopy, "");
COMMAND(entpaste, "");

void entset(char *what, int *a1, int *a2, int *a3, int *a4)
{
    if(noedit(true)) return;
    int type = findtype(what);
    groupedit(e.type=type;
              e.attr1=*a1;
              e.attr2=*a2;
              e.attr3=*a3;
              e.attr4=*a4;);
};

ICOMMAND(enthavesel,"",  intret(entgroup.length()));
ICOMMAND(entselect, "s", addgroup(e.type != ET_EMPTY && entgroup.find(n)<0 && execute(args[0])>0));
ICOMMAND(entloop,   "s", groupedit(((void)e, execute(args[0]))));
ICOMMAND(insel,     "",  entfocus(efocus, intret(pointinsel(sel, e.o))));
ICOMMAND(entget,    "",  entfocus(efocus, s_sprintfd(s)("%s %d %d %d %d", et->entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4);  result(s)));
COMMAND(entset, "siiii");


int findentity(int type, int index)
{
    const vector<extentity *> &ents = et->getents();
    for(int i = index; i<ents.length(); i++) if(ents[i]->type==type) return i;
    loopj(min(index, ents.length())) if(ents[j]->type==type) return j;
    return -1;
};

int spawncycle = -1, fixspawn = 2;

void findplayerspawn(dynent *d, int forceent)   // place at random spawn. also used by monsters!
{
    int pick = forceent;
    if(pick<0)
    {
        int r = fixspawn-->0 ? 5 : rnd(10)+1;
        loopi(r) spawncycle = findentity(ET_PLAYERSTART, spawncycle+1);
        pick = spawncycle;
    };
    if(pick!=-1)
    {
        d->o = et->getents()[pick]->o;
        d->yaw = et->getents()[pick]->attr1;
        d->pitch = 0;
        d->roll = 0;
    }
    else
    {
        d->o.x = d->o.y = d->o.z = 0.5f*getworldsize();
    };
    entinmap(d);
};

void splitocta(cube *c, int size)
{
    if(size <= VVEC_INT_MASK+1) return;
    loopi(8)
    {
        if(!c[i].children) c[i].children = newcubes(isempty(c[i]) ? F_EMPTY : F_SOLID);
        splitocta(c[i].children, size>>1);
    };
};

bool emptymap(int scale, bool force)    // main empty world creation routine
{
    if(!force && !editmode) 
    {
        conoutf("newmap only allowed in edit mode");
        return false;
    };

    clearoverrides();

    strncpy(hdr.head, "OCTA", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    hdr.worldsize = 1 << (scale<10 ? 10 : (scale>20 ? 20 : scale));

    s_strncpy(hdr.maptitle, "Untitled Map by Unknown", 128);
    hdr.waterlevel = -100000;
    memset(hdr.watercolour, 0, sizeof(hdr.watercolour));
    hdr.maple = 8;
    hdr.mapprec = 32;
    hdr.mapllod = 0;
    hdr.lightmaps = 0;
    memset(hdr.reserved1, 0, sizeof(hdr.reserved1));
    memset(hdr.reserved2, 0, sizeof(hdr.reserved2));
    texmru.setsize(0);
    freeocta(worldroot);
    et->getents().setsize(0);
    worldroot = newcubes(F_EMPTY);
    loopi(4) solidfaces(worldroot[i]);
    estartmap("");
    player->o.z += player->eyeheight+1;

    if(hdr.worldsize > VVEC_INT_MASK+1) splitocta(worldroot, hdr.worldsize>>1);

    resetlightmaps();
    clearlights();
    allchanged();

    overrideidents = true;
    execfile("data/default_map_settings.cfg");
    overrideidents = false;

    return true;
};

bool enlargemap(bool force)
{
    if(!force && !editmode)
    {
        conoutf("mapenlarge only allowed in edit mode");
        return false;
    };
    if(hdr.worldsize >= 1<<20) return false;

    hdr.worldsize *= 2;
    cube *c = newcubes(F_EMPTY);
    c[0].children = worldroot;
    loopi(3) solidfaces(c[i+1]);
    worldroot = c;

    if(hdr.worldsize > VVEC_INT_MASK+1) splitocta(worldroot, hdr.worldsize>>1);

    allchanged();

    return true;
};

void newmap(int *i) { if(emptymap(*i, false)) cl->newmap(max(*i, 0)); };
void mapenlarge() { if(enlargemap(false)) cl->newmap(-1); };
COMMAND(newmap, "i");
COMMAND(mapenlarge, "");

void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, bool local)
{
    if(et->getents().length()<=i)
    {
        while(et->getents().length()<i) et->getents().add(et->newentity())->type = ET_EMPTY;
        extentity *e = newentity(local, o, type, attr1, attr2, attr3, attr4);
        et->getents().add(e);
        addentity(i);
    }
    else
    {
        extentity &e = *et->getents()[i];
        removeentity(i);
        e.type = type;
        e.o = o;
        e.attr1 = attr1; e.attr2 = attr2; e.attr3 = attr3; e.attr4 = attr4;
        addentity(i);
    };
};

int getworldsize() { return hdr.worldsize; };
int getmapversion() { return hdr.version; };

bool insideworld(const vec &o)
{
    loopi(3) if(o.v[i]<0 || o.v[i]>=hdr.worldsize) return false;
    return true;
};

int triggertypes[NUMTRIGGERTYPES] =
{
    0,
    TRIG_ONCE,
    TRIG_RUMBLE,
    TRIG_TOGGLE,
    TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_MANY,
    TRIG_MANY | TRIG_RUMBLE,
    TRIG_MANY | TRIG_TOGGLE,
    TRIG_MANY | TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_AUTO_RESET | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_LOCKED | TRIG_RUMBLE,
    TRIG_DISAPPEAR,
    TRIG_DISAPPEAR | TRIG_RUMBLE,
    0, 0 /* reserved */
};

void resettriggers()
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        e.triggerstate = TRIGGER_RESET;
        e.lasttrigger = 0;
    };
};

void unlocktriggers(int tag, int oldstate = TRIGGER_RESET, int newstate = TRIGGERING)
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        if(e.attr4 == tag && e.triggerstate == oldstate && checktriggertype(e.attr3, TRIG_LOCKED))
        {
            if(newstate == TRIGGER_RESETTING && checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) continue;
            e.triggerstate = newstate;
            e.lasttrigger = lastmillis;
            if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
        };
    };
};

void trigger(int *tag, int *state)
{
    if(*state) unlocktriggers(*tag);
    else unlocktriggers(*tag, TRIGGERED, TRIGGER_RESETTING);
};

COMMAND(trigger, "ii");

VAR(triggerstate, -1, 0, 1);

void doleveltrigger(int trigger, int state)
{
    s_sprintfd(aliasname)("level_trigger_%d", trigger);
    if(identexists(aliasname))
    {
        triggerstate = state;
        execute(aliasname);
    };
};

void checktriggers()
{
    if(player->state != CS_ALIVE) return;
    vec o(player->o);
    o.z -= player->eyeheight;
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        switch(e.triggerstate)
        {
            case TRIGGERING:
            case TRIGGER_RESETTING:
                if(lastmillis-e.lasttrigger>=1000)
                {
                    if(e.attr4)
                    {
                        if(e.triggerstate == TRIGGERING) unlocktriggers(e.attr4);
                        else unlocktriggers(e.attr4, TRIGGERED, TRIGGER_RESETTING);
                    };
                    if(checktriggertype(e.attr3, TRIG_DISAPPEAR)) e.triggerstate = TRIGGER_DISAPPEARED;
                    else if(e.triggerstate==TRIGGERING && checktriggertype(e.attr3, TRIG_TOGGLE)) e.triggerstate = TRIGGERED;
                    else e.triggerstate = TRIGGER_RESET;
                };
                break;
            case TRIGGER_RESET:
                if(e.lasttrigger)
                {
                    if(checktriggertype(e.attr3, TRIG_AUTO_RESET|TRIG_MANY|TRIG_LOCKED) && e.o.dist(o)-player->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                        e.lasttrigger = 0;
                    break;
                }
                else if(e.o.dist(o)-player->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12)) break;
                else if(checktriggertype(e.attr3, TRIG_LOCKED))
                {
                    if(!e.attr4) break;
                    doleveltrigger(e.attr4, -1);
                    e.lasttrigger = lastmillis;
                    break;
                };
                e.triggerstate = TRIGGERING;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                et->trigger(e);
                if(e.attr4) doleveltrigger(e.attr4, 1);
                break;
            case TRIGGERED:
                if(e.o.dist(o)-player->radius<(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                {
                    if(e.lasttrigger) break;
                }
                else if(checktriggertype(e.attr3, TRIG_AUTO_RESET))
                {
                    if(lastmillis-e.lasttrigger<6000) break;
                }
                else if(checktriggertype(e.attr3, TRIG_MANY))
                {
                    e.lasttrigger = 0;
                    break;
                }
                else break;
                if(checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) break;
                e.triggerstate = TRIGGER_RESETTING;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                et->trigger(e);
                if(e.attr4) doleveltrigger(e.attr4, 0);
                break;
        };
    };
};

