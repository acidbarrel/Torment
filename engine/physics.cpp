// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "pch.h"
#include "engine.h"

const int MAXCLIPPLANES = 1000;

clipplanes clipcache[MAXCLIPPLANES], *nextclip = NULL;

void setcubeclip(cube &c, int x, int y, int z, int size)
{
    if(nextclip == NULL)
    {
        loopi(MAXCLIPPLANES)
        {
            clipcache[i].next = clipcache+i+1;
            clipcache[i].prev = clipcache+i-1;
            clipcache[i].backptr = NULL;
        };
        clipcache[MAXCLIPPLANES-1].next = clipcache;
        clipcache[0].prev = clipcache+MAXCLIPPLANES-1;
        nextclip = clipcache;
    };
    if(c.ext && c.ext->clip != NULL)
    {
        if(nextclip == c.ext->clip) return;
        clipplanes *&n = c.ext->clip->next;
        clipplanes *&p = c.ext->clip->prev;
        n->prev = p;
        p->next = n;
        n = nextclip;
        p = nextclip->prev;
        n->prev = c.ext->clip;
        p->next = c.ext->clip;
    }
    else
    {
        if(nextclip->backptr != NULL) *(nextclip->backptr) = NULL;
        nextclip->backptr = &ext(c).clip;
        c.ext->clip = nextclip;
        genclipplanes(c, x, y, z, size, *c.ext->clip);
        nextclip = nextclip->next;
    };
};

void freeclipplanes(cube &c)
{
    if(!c.ext || !c.ext->clip) return;
    c.ext->clip->backptr = NULL;
    c.ext->clip = NULL;
};

/////////////////////////  ray - cube collision ///////////////////////////////////////////////

static inline void pushvec(vec &o, const vec &ray, float dist)
{
    vec d(ray);
    d.mul(dist);
    o.add(d);
};

static inline bool pointinbox(const vec &v, const vec &bo, const vec &br)
{
    return v.x <= bo.x+br.x &&
           v.x >= bo.x-br.x &&
           v.y <= bo.y+br.y &&
           v.y >= bo.y-br.y &&
           v.z <= bo.z+br.z &&
           v.z >= bo.z-br.z;
};

bool pointincube(const clipplanes &p, const vec &v)
{
    if(!pointinbox(v, p.o, p.r)) return false;
    loopi(p.size) if(p.p[i].dist(v)>1e-3f) return false;
    return true;
};

bool raycubeintersect(const cube &c, const vec &o, const vec &ray, float &dist)
{
    clipplanes &p = *c.ext->clip;
    if(pointincube(p, o)) { dist = 0; return true; };

    loopi(p.size)
    {
        float a = ray.dot(p.p[i]);
        if(a>=0) continue;
        float f = -p.p[i].dist(o)/a;
        if(f + dist < 0) continue;

        vec d(o);
        pushvec(d, ray, f+0.1f);
        if(pointincube(p, d))
        {
            dist = f+0.1f;
            return true;
        };
    };
    return false;
};

extern int entselradius;
float hitentdist;
int  hitent;

static float disttoent(octaentities *oc, octaentities *last, const vec &o, const vec &ray, float radius, int mode, extentity *t)
{
    float dist = 1e16f, f = 0.0f;
    if(oc == last || oc == NULL) return dist;
    const vector<extentity *> &ents = et->getents();
    if((mode&RAY_POLY)==RAY_POLY)
        loopv(oc->mapmodels) if(!last || last->mapmodels.find(oc->mapmodels[i])<0)
        {
            extentity &e = *ents[oc->mapmodels[i]];
            if(!e.inoctanode || &e==t) continue;
            if(e.attr3 && (e.triggerstate == TRIGGER_DISAPPEARED || !checktriggertype(e.attr3, TRIG_COLLIDE) || e.triggerstate == TRIGGERED) && (mode&RAY_ENTS)!=RAY_ENTS) continue;
            if(!mmintersect(e, o, ray, radius, mode, f)) continue;
            if(f<dist) { hitentdist = dist = f; hitent = oc->mapmodels[i]; };
        };
    if((mode&RAY_ENTS)==RAY_ENTS)
        loopv(oc->other) if(!last || last->other.find(oc->other[i])<0)
        {
            extentity &e = *ents[oc->other[i]];
            if(!e.inoctanode || &e==t) continue;
            if(!raysphereintersect(e.o, entselradius, o, ray, f)) continue;
            if(f<dist) { hitentdist = dist = f; hitent = oc->other[i]; };
        };
    return dist;
};

int rayent(const vec &o, vec &ray)
{
    ray.normalize();
    hitent = -1;
    float d = raycube(o, ray, hitentdist = 1e20f, RAY_ENTS | RAY_POLY);
    if(hitentdist == d)
        return hitent;
    else
        return -1;
};

float raycubepos(const vec &o, vec &ray, vec &hitpos, float radius, int mode, int size)
{
    ray.normalize();
    hitpos = ray;
    float dist = raycube(o, ray, radius, mode, size);
    hitpos.mul(dist);
    hitpos.add(o);
    return dist;
};

float raycube(const vec &o, const vec &ray, float radius, int mode, int size, extentity *t)
{
    octaentities *oclast = NULL;
    float dist = 0, dent = 1e16f;
    cube *last = NULL;
    vec v = o;
    if(ray.iszero()) return dist;

    static cube *levels[32];
    levels[0] = worldroot;
    int l = 0, lsize = hdr.worldsize;
    ivec lo(0, 0, 0);

    if(!insideworld(v))
    {
        float disttoworld = 1e16f, exitworld = 1e16f;
        loopi(3) if(ray[i]!=0)
        {
            float c = v[i];
            if(c>=0 && c<hdr.worldsize)
            {
                float d = (float(ray[i]>0?hdr.worldsize:0)-c)/ray[i];
                exitworld = min(exitworld, d);
            }
            else
            {
                float d = (float(ray[i]>0?0:hdr.worldsize)-c)/ray[i];
                if(d<0) return (radius>0?radius:-1);
                disttoworld = min(disttoworld, 0.1f + d);
            };
        };
        if(disttoworld > exitworld) return (radius>0?radius:-1);
        pushvec(v, ray, disttoworld);
        dist += disttoworld;
    };

    int x = int(v.x), y = int(v.y), z = int(v.z);
    for(;;)
    {
        cube *lc = levels[l];
        for(;;)
        {
            lsize >>= 1;
            if(z>=lo.z+lsize) { lo.z += lsize; lc += 4; };
            if(y>=lo.y+lsize) { lo.y += lsize; lc += 2; };
            if(x>=lo.x+lsize) { lo.x += lsize; lc += 1; };
            if(lc->ext && lc->ext->ents && (mode&RAY_BB) && dent > 1e15f)
            {
                dent = disttoent(lc->ext->ents, oclast, o, ray, radius, mode, t);
                if((mode&RAY_SHADOW) && dent < 1e15f) return min(dent, dist);
                oclast = lc->ext->ents;
            };
            if(lc->children==NULL) break;
            lc = lc->children;
            levels[++l] = lc;
        };

        cube &c = *lc;
        if((dist>0 || !(mode&RAY_SKIPFIRST)) &&
           (((mode&RAY_CLIPMAT) && c.ext && isclipped(c.ext->material) && c.ext->material != MAT_CLIP) ||
            ((mode&RAY_EDITMAT) && c.ext && c.ext->material != MAT_AIR) ||
            (!(mode&RAY_PASS) && lsize==size && !isempty(c)) ||
            isentirelysolid(c) ||
            dent < dist ||
            last==&c))
        {
            if(last==&c && radius>0) dist = radius;
            return min(dent, dist);
        }
        else if(!isempty(c))
        {
            float f = 0;
            setcubeclip(c, lo.x, lo.y, lo.z, lsize);
            if(raycubeintersect(c, v, ray, f) && (dist+f>0 || !(mode&RAY_SKIPFIRST)))
                return min(dent, dist+f);
        };

        float disttonext = 1e16f;
        loopi(3) if(ray[i]!=0)
        {
            float d = (float(lo[i]+(ray[i]>0?lsize:0))-v[i])/ray[i];
            if(d >= 0) disttonext = min(disttonext, 0.1f + d);
        };
        pushvec(v, ray, disttonext);
        dist += disttonext;
        last = &c;

        if(radius>0 && dist>=radius) return min(dent, dist);

        x = int(v.x);
        y = int(v.y);
        z = int(v.z);

        for(;;)
        {
            lo.x &= ~lsize;
            lo.y &= ~lsize;
            lo.z &= ~lsize;
            lsize <<= 1;
            if(x<lo.x+lsize && y<lo.y+lsize && z<lo.z+lsize)
            {
                if(x>=lo.x && y>=lo.y && z>=lo.z) break;
            };
            if(!l) break;
            --l;
        };
    };
};

/////////////////////////  entity collision  ///////////////////////////////////////////////

// info about collisions
bool inside, hitplayer; // whether an internal collision happened, whether the collision hit a player
vec wall; // just the normal vectors.
float walldistance;
const float STAIRHEIGHT = 4.1f;
const float FLOORZ = 0.867f;
const float SLOPEZ = 0.5f;
const float WALLZ = 0.2f;
const float JUMPVEL = 125.0f;
const float GRAVITY = 200.0f;
const float STEPSPEED = 1.0f;

bool rectcollide(physent *d, const vec &dir, const vec &o, float xr, float yr,  float hi, float lo, uchar visible = 0xFF, bool collideonly = true)
{
    if(collideonly && !visible) return true;
    vec s(d->o);
    s.sub(o);
    xr += d->radius;
    yr += d->radius;
    walldistance = -1e10f;
    float zr = s.z>0 ? d->eyeheight+hi : d->aboveeye+lo;
    float ax = fabs(s.x)-xr;
    float ay = fabs(s.y)-yr;
    float az = fabs(s.z)-zr;
    if(ax>0 || ay>0 || az>0) return true;
    wall.x = wall.y = wall.z = 0;
#define TRYCOLLIDE(dim, N, P) \
    { \
        if(s.dim<0) { if((dir.iszero() || dir.dim>0) && (N)) { walldistance = a ## dim; wall.dim = -1; return false; }; } \
        else if((dir.iszero() || dir.dim<0) && (P)) { walldistance = a ## dim; wall.dim = 1; return false; }; \
    }
    if(ax>ay && ax>az) TRYCOLLIDE(x, visible&(1<<O_LEFT), visible&(1<<O_RIGHT));
    if(ay>az) TRYCOLLIDE(y, visible&(1<<O_BACK), visible&(1<<O_FRONT));
    TRYCOLLIDE(z,
        (d->type >= ENT_CAMERA || az >= -(d->eyeheight+d->aboveeye)/4.0f) && (visible&(1<<O_BOTTOM)),
        (d->type >= ENT_CAMERA || az >= -(d->eyeheight+d->aboveeye)/2.0f) && (visible&(1<<O_TOP)));
    if(collideonly) inside = true;
    return collideonly;
};

#define DYNENTCACHESIZE 1024

static uint dynentframe = 0;

static struct dynentcacheentry
{
    int x, y;
    uint frame;
    vector<physent *> dynents;
} dynentcache[DYNENTCACHESIZE];

void cleardynentcache()
{
    dynentframe++;
    if(!dynentframe || dynentframe == 1) loopi(DYNENTCACHESIZE) dynentcache[i].frame = 0;
    if(!dynentframe) dynentframe = 1;
};

VARF(dynentsize, 6, 8, 12, cleardynentcache());

#define DYNENTHASH(x, y) (((((x)^(y))<<5) + (((x)^(y))>>5)) & (DYNENTCACHESIZE - 1))

const vector<physent *> &checkdynentcache(int x, int y)
{
    dynentcacheentry &dec = dynentcache[DYNENTHASH(x, y)];
    if(dec.x == x && dec.y == y && dec.frame == dynentframe) return dec.dynents;
    dec.x = x;
    dec.y = y;
    dec.frame = dynentframe;
    dec.dynents.setsize(0);
    int numdyns = cl->numdynents(), dsize = 1<<dynentsize, dx = x<<dynentsize, dy = y<<dynentsize;
    loopi(numdyns)
    {
        dynent *d = cl->iterdynents(i);
        if(!d || d->state != CS_ALIVE ||
           d->o.x+d->radius <= dx || d->o.x-d->radius >= dx+dsize ||
           d->o.y+d->radius <= dy || d->o.y-d->radius >= dy+dsize)
            continue;
        dec.dynents.add(d);
    };
    return dec.dynents;
};

void updatedynentcache(physent *d)
{
    for(int x = int(max(d->o.x-d->radius, 0))>>dynentsize, ex = int(min(d->o.x+d->radius, hdr.worldsize-1))>>dynentsize; x <= ex; x++)
    for(int y = int(max(d->o.y-d->radius, 0))>>dynentsize, ey = int(min(d->o.y+d->radius, hdr.worldsize-1))>>dynentsize; y <= ey; y++)
    {
        dynentcacheentry &dec = dynentcache[DYNENTHASH(x, y)];
        if(dec.x != x || dec.y != y || dec.frame != dynentframe || dec.dynents.find(d) >= 0) continue;
        dec.dynents.add(d);
    };
};

bool overlapsdynent(const vec &o, float radius)
{
    for(int x = int(max(o.x-radius, 0))>>dynentsize, ex = int(min(o.x+radius, hdr.worldsize-1))>>dynentsize; x <= ex; x++)
    for(int y = int(max(o.y-radius, 0))>>dynentsize, ey = int(min(o.y+radius, hdr.worldsize-1))>>dynentsize; y <= ey; y++)
    {
        const vector<physent *> &dynents = checkdynentcache(x, y);
        loopv(dynents)
        {
            physent *d = dynents[i];
            if(o.dist(d->o)-d->radius < radius) return true;
        };
    };
    return false;
};

bool plcollide(physent *d, const vec &dir)    // collide with player or monster
{
    if(d->state != CS_ALIVE) return true;
    for(int x = int(max(d->o.x-d->radius, 0))>>dynentsize, ex = int(min(d->o.x+d->radius, hdr.worldsize-1))>>dynentsize; x <= ex; x++)
    for(int y = int(max(d->o.y-d->radius, 0))>>dynentsize, ey = int(min(d->o.y+d->radius, hdr.worldsize-1))>>dynentsize; y <= ey; y++)
    {
        const vector<physent *> &dynents = checkdynentcache(x, y);
        loopv(dynents)
        {
            physent *o = dynents[i];
            if(o==d || (o==player && d->type==ENT_CAMERA) || d->o.reject(o->o, d->radius+o->radius)) continue;
            if(!rectcollide(d, dir, o->o, o->radius, o->radius, o->aboveeye, o->eyeheight))
            {
                hitplayer = true;
                return false;
            };
        };
    };
    return true;
};

void rotatebb(vec &center, vec &radius, int yaw)
{
    yaw = (yaw+7)-(yaw+7)%15;
    yaw = 180-yaw;
    if(yaw<0) yaw += 360;
    switch(yaw)
    {
        case 0: break;
        case 180: 
            center.x = -center.x;
            center.y = -center.y;
            break;
        case 90:
            swap(float, radius.x, radius.y);
            swap(float, center.x, center.y);
            center.x = -center.x;
            break;
        case 270: 
            swap(float, radius.x, radius.y); 
            swap(float, center.x, center.y);
            center.y = -center.y;
            break;
        default: 
            radius.x = radius.y = max(radius.x, radius.y) + max(fabs(center.x), fabs(center.y));
            center.x = center.y = 0.0f;
            break;
    };
};

bool mmcollide(physent *d, const vec &dir, octaentities &oc)               // collide with a mapmodel
{
    const vector<extentity *> &ents = et->getents();
    loopv(oc.mapmodels)
    {
        extentity &e = *ents[oc.mapmodels[i]];
        if(e.attr3 && (e.triggerstate == TRIGGER_DISAPPEARED || !checktriggertype(e.attr3, TRIG_COLLIDE) || e.triggerstate == TRIGGERED)) continue;
        model *m = loadmodel(NULL, e.attr2);
        if(!m || !m->collide) continue;
        vec center, radius;
        m->collisionbox(0, center, radius);
        center.z -= radius.z;
        radius.z *= 2;
        rotatebb(center, radius, e.attr1);
        if(!rectcollide(d, dir, center.add(e.o), radius.x, radius.y, radius.z, 0)) return false;
    };
    return true;
};

bool cubecollide(physent *d, const vec &dir, float cutoff, cube &c, int x, int y, int z, int size) // collide with cube geometry
{
    if(isentirelysolid(c) || (c.ext && (d->type<ENT_CAMERA || c.ext->material != MAT_CLIP) && isclipped(c.ext->material)))
    {
        int s2 = size>>1;
        vec o = vec(x+s2, y+s2, z+s2);
        vec r = vec(s2, s2, s2);
        return rectcollide(d, dir, o, r.x, r.y, r.z, r.z, isentirelysolid(c) ? (c.ext ? c.ext->visible : 0) : 0xFF);
    };

    setcubeclip(c, x, y, z, size);
    clipplanes &p = *c.ext->clip;

    float r = d->radius,
          zr = (d->aboveeye+d->eyeheight)/2;
    vec o(d->o), *w = &wall;
    o.z += zr - d->eyeheight;

    if(rectcollide(d, dir, p.o, p.r.x, p.r.y, p.r.z, p.r.z, c.ext->visible, !p.size)) return true;

    if(p.size)
    {
        float m = walldistance;
        loopi(p.size)
        {
            plane &f = p.p[i];
            float dist = f.dist(o) - (fabs(f.x*r)+fabs(f.y*r)+fabs(f.z*zr));
            if(dist > 0) return true;
            if(dist > m)
            {
                if(!dir.iszero())
                {
                    if(f.dot(dir) >= -cutoff) continue;
                    if(d->type < ENT_CAMERA)
                    {
                        if(dir.z < 0 && f.z > 0 && dist < -(d->eyeheight+d->aboveeye)/2.0f) continue;
                        else if(dir.z > 0 && f.z < 0 && dist < -(d->eyeheight+d->aboveeye)/4.0f) continue;
                    };
                };
                w = &p.p[i];
                m = dist;
            };
        };
        wall = *w;
        if(wall.iszero())
        {
            inside = true;
            return true;
        };
    };
    return false;
};

bool octacollide(physent *d, const vec &dir, float cutoff, const ivec &bo, const ivec &bs, cube *c, const ivec &cor, int size) // collide with octants
{
    loopoctabox(cor, size, bo, bs)
    {
        if(c[i].ext && c[i].ext->ents) if(!mmcollide(d, dir, *c[i].ext->ents)) return false;
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children)
        {
            if(!octacollide(d, dir, cutoff, bo, bs, c[i].children, o, size>>1)) return false;
        }
        else if(c[i].ext && c[i].ext->material!=MAT_NOCLIP && (!isempty(c[i]) || ((d->type<ENT_CAMERA || c[i].ext->material != MAT_CLIP) && isclipped(c[i].ext->material))))
        {
            if(!cubecollide(d, dir, cutoff, c[i], o.x, o.y, o.z, size)) return false;
        };
    };
    return true;
};

// all collision happens here
bool collide(physent *d, const vec &dir, float cutoff)
{
    inside = hitplayer = false;
    wall.x = wall.y = wall.z = 0;
    ivec bo(int(d->o.x-d->radius), int(d->o.y-d->radius), int(d->o.z-d->eyeheight)),
         bs(int(d->radius)*2, int(d->radius)*2, int(d->eyeheight+d->aboveeye));
    bs.add(2);  // guard space for rounding errors
    if(!octacollide(d, dir, cutoff, bo, bs, worldroot, ivec(0, 0, 0), hdr.worldsize>>1)) return false; // collide with world
    return plcollide(d, dir);
};

void slopegravity(float g, const vec &slope, vec &gvec)
{
    float k = slope.z*g/(slope.x*slope.x + slope.y*slope.y);
    gvec.x = slope.x*k;
    gvec.y = slope.y*k;
    gvec.z = -g;
};

void slideagainst(physent *d, vec &dir, const vec &obstacle)
{
    vec wdir(obstacle), wvel(obstacle);
    wdir.z = wvel.z = d->physstate >= PHYS_SLOPE ? 0.0f : min(obstacle.z, 0.0f);
//    if(obstacle.z < 0.0f && dir.z > 0.0f) dir.z = d->vel.z = 0.0f;
    wdir.mul(obstacle.dot(dir));
    wvel.mul(obstacle.dot(d->vel));
    dir.sub(wdir);
    d->vel.sub(wvel);
};

void switchfloor(physent *d, vec &dir, bool landing, const vec &floor)
{
    if(d->physstate == PHYS_FALL || d->floor.z < FLOORZ)
    {
        if(landing)
        {
            if(floor.z >= FLOORZ)
            {
                if(d->vel.z + d->gravity.z > 0) d->vel.add(d->gravity);
                else if(d->vel.z > 0) d->vel.z = 0.0f;
                d->gravity = vec(0, 0, 0);
            }
            else
            {
                float gmag = d->gravity.magnitude();
                if(gmag > 1e-4f)
                {
                    vec g;
                    slopegravity(-d->gravity.z, floor, g);
                    if(d->physstate == PHYS_FALL || d->floor != floor)
                    {
                        float c = d->gravity.dot(floor)/gmag;
                        g.normalize();
                        g.mul(min(1.0f+c, 1.0f)*gmag);
                    };
                    d->gravity = g;
                };
            };
        };
    };

    if(((d->physstate == PHYS_SLIDE || (d->physstate == PHYS_FALL && floor.z < 1.0f)) && landing) ||
        (d->physstate >= PHYS_SLOPE && fabs(dir.dot(d->floor)/dir.magnitude()) < 0.01f))
    {
        if(floor.z > 0 && floor.z < WALLZ) { slideagainst(d, dir, floor); return; };

        float dmag = dir.magnitude(), dz = -(dir.x*floor.x + dir.y*floor.y)/floor.z;
        dir.z = dz;
        float dfmag = dir.magnitude();
        if(dfmag > 0) dir.mul(dmag/dfmag);

        float vmag = d->vel.magnitude(), vz = -(d->vel.x*floor.x + d->vel.y*floor.y)/floor.z;
        d->vel.z = vz;
        float vfmag = d->vel.magnitude();
        if(vfmag > 0) d->vel.mul(vmag/vfmag);
    };
};

bool trystepup(physent *d, vec &dir, float maxstep)
{
    vec old(d->o);
    /* check if there is space atop the stair to move to */
    if(d->physstate != PHYS_STEP_UP)
    {
        d->o.add(dir);
        d->o.z += maxstep + 0.1f;
        if(!collide(d))
        {
            d->o = old;
            return false;
        };
    };
    /* try stepping up */
    d->o = old;
    d->o.z += dir.magnitude()*STEPSPEED;
    if(collide(d, vec(0, 0, 1)))
    {
        if(d->physstate == PHYS_FALL)
        {
            d->timeinair = 0;
            d->floor = vec(0, 0, 1);
            switchfloor(d, dir, true, d->floor);
        };
        d->physstate = PHYS_STEP_UP;
        return true;
    };
    d->o = old;
    return false;
};

#if 0
bool trystepdown(physent *d, vec &dir, float step, float a, float b)
{
    vec old(d->o);
    vec dv(dir.x*a, dir.y*a, -step*b), v(dv);
    v.mul(STAIRHEIGHT/(step*b));
    d->o.add(v);
    if(!collide(d, vec(0, 0, -1), SLOPEZ))
    {
        d->o = old;
        d->o.add(dv);
        if(collide(d, vec(0, 0, -1))) return true;
    };
    d->o = old;
    return false;
};
#endif

void falling(physent *d, vec &dir, const vec &floor)
{
#if 0
    if(d->physstate >= PHYS_FLOOR && (floor.z == 0.0f || floor.z == 1.0f))
    {
        vec moved(d->o);
        d->o.z -= STAIRHEIGHT + 0.1f;
        if(!collide(d, vec(0, 0, -1), SLOPEZ))
        {
            d->o = moved;
            d->physstate = PHYS_STEP_DOWN;
            return;
        }
        else d->o = moved;
    };
#endif
    bool sliding = floor.z > 0.0f && floor.z < SLOPEZ;
    switchfloor(d, dir, sliding, sliding ? floor : vec(0, 0, 1));
    if(sliding)
    {
        if(d->timeinair > 0) d->timeinair = 0;
        d->physstate = PHYS_SLIDE;
        d->floor = floor;
    }
    else d->physstate = PHYS_FALL;
};

void landing(physent *d, vec &dir, const vec &floor)
{
    if(d->physstate == PHYS_FALL)
    {
        d->timeinair = 0;
        if(dir.z < 0.0f) dir.z = d->vel.z = 0.0f;
    }
    switchfloor(d, dir, true, floor);
    if(floor.z >= FLOORZ) d->physstate = PHYS_FLOOR;
    else d->physstate = PHYS_SLOPE;
    d->floor = floor;
};

bool findfloor(physent *d, bool collided, const vec &obstacle, bool &slide, vec &floor)
{
    bool found = false;
    vec moved(d->o);
    d->o.z -= 0.1f;
    if(!collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE ? SLOPEZ : FLOORZ))
    {
        floor = wall;
        found = true;
    }
    else if(collided && obstacle.z >= SLOPEZ)
    {
        floor = obstacle;
        found = true;
        slide = false;
    }
    else
    {
        if(d->physstate == PHYS_STEP_UP || d->physstate == PHYS_SLIDE)
        {
            if(!collide(d, vec(0, 0, -1)) && wall.z > 0.0f)
            {
                floor = wall;
                if(floor.z > SLOPEZ) found = true;
            };
        }
        else
        {
            d->o.z -= d->radius;
            if(d->physstate >= PHYS_SLOPE && d->floor.z < 1.0f && !collide(d, vec(0, 0, -1)))
            {
                floor = wall;
                if(floor.z >= SLOPEZ && floor.z < 1.0f) found = true;
            };
        };
        if(collided && (!found || obstacle.z > floor.z)) floor = obstacle;
    };
    d->o = moved;
    return found;
};

bool move(physent *d, vec &dir)
{
    vec old(d->o);
#if 0
    if(d->physstate == PHYS_STEP_DOWN && dir.z <= 0.0f && (d->move || d->strafe))
    {
        float step = dir.magnitude()*STEPSPEED;
        if(trystepdown(d, dir, step, 0.75f, 0.25f)) return true;
        if(trystepdown(d, dir, step, 0.5f, 0.5f)) return true;
        if(trystepdown(d, dir, step, 0.25f, 0.75f)) return true;
        d->o.z -= step;
        if(collide(d, vec(0, 0, -1))) return true;
        d->o = old;
    };
#endif
    bool collided = false;
    vec obstacle;
    d->o.add(dir);
    if(!collide(d, dir))
    {
        if(d->type == ENT_CAMERA) return false;
        obstacle = wall;
        d->o = old;
        d->o.z -= (d->physstate >= PHYS_SLOPE && d->floor.z < 1.0f ? d->radius+0.1f : STAIRHEIGHT);
        if((d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR) || (!collide(d, vec(0, 0, -1), SLOPEZ) && (d->physstate == PHYS_STEP_UP || wall.z == 1.0f)))
        {
            d->o = old;
            float floorz = (d->physstate == PHYS_SLOPE || d->physstate == PHYS_FLOOR ? d->floor.z : wall.z);
            if(trystepup(d, dir, floorz < 1.0f ? d->radius+0.1f : STAIRHEIGHT)) return true;
        }
        else d->o = old;
        /* can't step over the obstacle, so just slide against it */
        collided = true;
    }
    else if(inside && d->type != ENT_PLAYER)
    {
        d->o = old;
        if(d->type == ENT_AI) d->blocked = true;
        return false;
    };
    vec floor(0, 0, 0);
    bool slide = collided && obstacle.z < 1.0f,
         found = findfloor(d, collided, obstacle, slide, floor);
    if(slide)
    {
        slideagainst(d, dir, obstacle);
        if(d->type == ENT_AI) d->blocked = true;
    };
    if(found)
    {
        if(d->type == ENT_CAMERA) return false;
        landing(d, dir, floor);
    }
    else falling(d, dir, floor);
    return !collided;
};

bool bounce(physent *d, float secs, float elasticity, float waterfric)
{
    cube &c = lookupcube(int(d->o.x), int(d->o.y), int(d->o.z));
    bool water = c.ext && c.ext->material == MAT_WATER;
    if(water)
    {
        if(d->vel.z > 0 && d->vel.z + d->gravity.z < 0) d->vel.z = 0.0f;
        d->gravity.z = -4.0f*GRAVITY*secs;
    }
    else d->gravity.z -= GRAVITY*secs;
    if(water) d->vel.mul(1.0f - secs/waterfric);
    vec old(d->o);
    loopi(2)
    {
        vec dir(d->vel);
        if(water) dir.mul(0.5f);
        dir.add(d->gravity);
        dir.mul(secs);
        d->o.add(dir);
        if(collide(d, dir))
        {
            if(inside)
            {
                d->o = old;
                d->gravity.mul(-elasticity);
                d->vel.mul(-elasticity);
            };
            break;
        }
        else if(hitplayer) break;
        d->o = old;
        vec dvel(d->vel), wvel(wall);
        dvel.add(d->gravity);
        float c = wall.dot(dvel),
              k = 1.0f + (1.0f-elasticity)*c/dvel.magnitude();
        wvel.mul(elasticity*2.0f*c);
        d->gravity.mul(k);
        d->vel.mul(k);
        d->vel.sub(wvel);
    };
    if(d->physstate != PHYS_BOUNCE)
    {
        // make sure bouncers don't start inside geometry
        if(d->o == old) return true;
        d->physstate = PHYS_BOUNCE;
    };
    return hitplayer;
};

void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space)
{
    float rad = obstacle->radius+d->radius;
    vec bbmin(obstacle->o);
    bbmin.x -= rad;
    bbmin.y -= rad;
    bbmin.z -= obstacle->eyeheight+d->aboveeye;
    bbmin.sub(space);
    vec bbmax(obstacle->o);
    bbmax.x += rad;
    bbmax.y += rad;
    bbmax.z += obstacle->aboveeye+d->eyeheight;
    bbmax.add(space);

    loopi(3) if(d->o[i] <= bbmin[i] || d->o[i] >= bbmax[i]) return;

    float mindist = 1e16f;
    loopi(3) if(dir[i] != 0)
    {
        float dist = ((dir[i] > 0 ? bbmax[i] : bbmin[i]) - d->o[i]) / dir[i];
        mindist = min(mindist, dist);
    };
    if(mindist >= 0.0f && mindist < 1e15f) d->o.add(vec(dir).mul(mindist));
};

void dropenttofloor(entity *e)
{
    if(!insideworld(e->o)) return;
    vec v(0.0001f, 0.0001f, -1);
    v.normalize();
    if(raycube(e->o, v, hdr.worldsize) >= hdr.worldsize) return;
    physent d;
    d.type = ENT_CAMERA;
    d.o = e->o;
    d.vel = vec(0, 0, -1);
    d.radius = 1.0f;
    d.eyeheight = et->dropheight(*e);
    d.aboveeye = 1.0f;
    loopi(hdr.worldsize) if(!move(&d, v)) break;
    e->o = d.o;
};

void phystest()
{
    static const char *states[] = {"float", "fall", "slide", "slope", "floor", "step up", "step down", "bounce"};
    printf ("PHYS(pl): %s, air %d, floor: (%f, %f, %f), vel: (%f, %f, %f), g: (%f, %f, %f)\n", states[player->physstate], player->timeinair, player->floor.x, player->floor.y, player->floor.z, player->vel.x, player->vel.y, player->vel.z, player->gravity.x, player->gravity.y, player->gravity.z);
    printf ("PHYS(cam): %s, air %d, floor: (%f, %f, %f), vel: (%f, %f, %f), g: (%f, %f, %f)\n", states[camera1->physstate], camera1->timeinair, camera1->floor.x, camera1->floor.y, camera1->floor.z, camera1->vel.x, camera1->vel.y, camera1->vel.z, camera1->gravity.x, camera1->gravity.y, camera1->gravity.z);
}

COMMAND(phystest, "");

void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m, bool floating)
{
    m.x = move*sinf(RAD*(yaw));
    m.y = move*-cosf(RAD*(yaw));

    if(floating)
    {
        m.x *= cosf(RAD*pitch);
        m.y *= cosf(RAD*pitch);
        m.z = move*sinf(RAD*pitch);
    }
    else
    {
        m.z = 0;
    };

    m.x += strafe*-cosf(RAD*(yaw));
    m.y += strafe*-sinf(RAD*(yaw));
};

VARP(maxroll, 0, 3, 20);

void modifyvelocity(physent *pl, int moveres, bool local, bool water, bool floating, int curtime)
{
    if(floating)
    {
        if(pl->jumpnext)
        {
            pl->jumpnext = false;
            pl->vel.z = JUMPVEL;
        };
    }
    else
    if(pl->physstate >= PHYS_SLOPE || water)
    {
        if(water && pl->timeinair > 0)
        {
            pl->timeinair = 0;
            pl->vel.z = 0;
        };
        if(pl->jumpnext)
        {
            pl->jumpnext = false;

            pl->vel.add(vec(pl->vel).mul(0.3f));        // EXPERIMENTAL
            pl->vel.z = JUMPVEL; // physics impulse upwards
            if(water) { pl->vel.x /= 8.0f; pl->vel.y /= 8.0f; }; // dampen velocity change even harder, gives correct water feel

            cl->physicstrigger(pl, local, 1, 0);
        };
    }
    else
    {
        pl->timeinair += curtime;
    };

    vec m(0.0f, 0.0f, 0.0f);
    if(pl->move || pl->strafe)
    {
        vecfromyawpitch(pl->yaw, pl->pitch, pl->move, pl->strafe, m, floating || water || pl->type==ENT_CAMERA);

        if(!floating && pl->physstate >= PHYS_SLIDE)
        {
            /* move up or down slopes in air
             * but only move up slopes in water
             */
            float dz = -(m.x*pl->floor.x + m.y*pl->floor.y)/pl->floor.z;
            if(water) m.z = max(m.z, dz);
            else if(pl->floor.z >= WALLZ) m.z = dz;
        };

        m.normalize();
    };

    vec d(m);
    d.mul(pl->maxspeed);
    float friction = water && !floating ? 20.0f : (pl->physstate >= PHYS_SLOPE || floating ? 6.0f : 30.f);
    float fpsfric = friction/curtime*20.0f;

    pl->vel.mul(fpsfric-1);
    pl->vel.add(d);
    pl->vel.div(fpsfric);
};

void modifygravity(physent *pl, bool water, float secs)
{
    vec g(0, 0, 0);
    if(pl->physstate == PHYS_FALL) g.z -= GRAVITY*secs;
    else if(!pl->floor.iszero() && pl->floor.z < FLOORZ)
    {
        float c = min(FLOORZ - pl->floor.z, FLOORZ-SLOPEZ)/(FLOORZ-SLOPEZ);
        slopegravity(GRAVITY*secs*c, pl->floor, g);
    };
    if(water) pl->gravity = pl->move || pl->strafe ? vec(0, 0, 0) : g.mul(4.0f);
    else pl->gravity.add(g);
};

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

bool moveplayer(physent *pl, int moveres, bool local, int curtime)
{
    cube &c = lookupcube(int(pl->o.x), int(pl->o.y), int(pl->o.z));
    bool water = c.ext && c.ext->material == MAT_WATER;
    bool floating = (editmode && local) || pl->state==CS_EDITING || pl->state==CS_SPECTATOR;
    float secs = curtime/1000.f;

    // apply any player generated changes in velocity
    modifyvelocity(pl, moveres, local, water, floating, curtime);

    // apply gravity
    if(!floating && pl->type!=ENT_CAMERA) modifygravity(pl, water, secs);

    vec d(pl->vel);
    if(!floating && pl->type!=ENT_CAMERA && water) d.mul(0.5f);
    d.add(pl->gravity);
    d.mul(secs);

    pl->blocked = false;
    pl->moving = true;

    if(floating)                // just apply velocity
    {
        if(pl->physstate != PHYS_FLOAT)
        {
            pl->physstate = PHYS_FLOAT;
            pl->timeinair = 0;
            pl->gravity = vec(0, 0, 0);
        };
        pl->o.add(d);
    }
    else                        // apply velocity with collision
    {
        const float f = 1.0f/moveres;
        const int timeinair = pl->timeinair;
        int collisions = 0;

        d.mul(f);
        loopi(moveres) if(!move(pl, d)) { if(pl->type==ENT_CAMERA) return false; if(++collisions<5) i--; }; // discrete steps collision detection & sliding
        if(timeinair > 800 && !pl->timeinair) // if we land after long time must have been a high jump, make thud sound
        {
            cl->physicstrigger(pl, local, -1, 0);
        };
    };

    if(pl->state==CS_ALIVE) updatedynentcache(pl);

    if(!pl->timeinair && pl->physstate >= PHYS_FLOOR && pl->vel.squaredlen() < 1e-4f && pl->gravity.iszero()) pl->moving = false;

    // automatically apply smooth roll when strafing

    if(pl->strafe==0)
    {
        pl->roll = pl->roll/(1+(float)sqrtf((float)curtime)/25);
    }
    else
    {
        pl->roll += pl->strafe*curtime/-30.0f;
        if(pl->roll>maxroll) pl->roll = (float)maxroll;
        if(pl->roll<-maxroll) pl->roll = (float)-maxroll;
    };

    // play sounds on water transitions

    if(pl->type!=ENT_CAMERA)
    {
        cube &c = lookupcube((int)pl->o.x, (int)pl->o.y, (int)pl->o.z+1);
        bool inwater = c.ext && c.ext->material == MAT_WATER;
        if(!pl->inwater && inwater) cl->physicstrigger(pl, local, 0, -1);
        else if(pl->inwater && !inwater) cl->physicstrigger(pl, local, 0, 1);
        pl->inwater = inwater;
    };

    return true;
};

VAR(minframetime, 5, 10, 20);

int physicsfraction = 0, physicsrepeat = 0;

void physicsframe()          // optimally schedule physics frames inside the graphics frames
{
    if(curtime>=minframetime)
    {
        int faketime = curtime+physicsfraction;
        physicsrepeat = faketime/minframetime;
        physicsfraction = faketime%minframetime;
    }
    else
    {
        physicsrepeat = 1;
    };
    cleardynentcache();
};

void moveplayer(physent *pl, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(pl, moveres, local, min(curtime, minframetime));
    if(pl->o.z<0 && pl->state==CS_ALIVE) cl->worldhurts(pl, 400);
};

bool intersect(physent *d, vec &from, vec &to)   // if lineseg hits entity bounding box
{
    vec v = to, w = d->o, *p;
    v.sub(from);
    w.sub(from);
    float c1 = w.dot(v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = v.dot(v);
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            v.mul(f);
            v.add(from);
            p = &v;
        };
    };

    return p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight;
};

#define dir(name,v,d,s,os) ICOMMAND(name, "D", { player->s = args!=NULL; player->v = player->s ? d : (player->os ? -(d) : 0); });

dir(backward, move,   -1, k_down,  k_up);
dir(forward,  move,    1, k_up,    k_down);
dir(left,     strafe,  1, k_left,  k_right);
dir(right,    strafe, -1, k_right, k_left);

ICOMMAND(jump,   "D", { if(cl->canjump()) player->jumpnext = args!=NULL; });
ICOMMAND(attack, "D", { cl->doattack(args!=NULL); });

VARP(sensitivity, 0, 10, 1000);
VARP(sensitivityscale, 1, 1, 100);
VARP(invmouse, 0, 0, 1);

void fixcamerarange()
{
    const float MAXPITCH = 90.0f;
    if(camera1->pitch>MAXPITCH) camera1->pitch = MAXPITCH;
    if(camera1->pitch<-MAXPITCH) camera1->pitch = -MAXPITCH;
    while(camera1->yaw<0.0f) camera1->yaw += 360.0f;
    while(camera1->yaw>=360.0f) camera1->yaw -= 360.0f;
};

void mousemove(int dx, int dy)
{
    const float SENSF = 33.0f;     // try match quake sens
    camera1->yaw += (dx/SENSF)*(sensitivity/(float)sensitivityscale);
    camera1->pitch -= (dy/SENSF)*(sensitivity/(float)sensitivityscale)*(invmouse ? -1 : 1);
    fixcamerarange();
    if(camera1!=player && player->state!=CS_DEAD)
    {
        player->yaw = camera1->yaw;
        player->pitch = camera1->pitch;
    };
};

void entinmap(dynent *d)        // brute force but effective way to find a free spawn spot in the map
{
    d->o.z += d->eyeheight;     // pos specified is at feet
    vec orig = d->o;
    loopi(100)                  // try max 100 times
    {
        if(collide(d) && !inside) return;
        d->o = orig;
        d->o.x += (rnd(21)-10)*i/5;  // increasing distance
        d->o.y += (rnd(21)-10)*i/5;
        d->o.z += (rnd(21)-10)*i/5;
    };
    conoutf("can't find entity spawn spot! (%d, %d)", d->o.x, d->o.y);
    // leave ent at original pos, possibly stuck
    d->o = orig;
};

void updatephysstate(physent *d)
{
    if(d->physstate == PHYS_FALL) return;
    d->timeinair = 0;
    vec old(d->o);
    /* Attempt to reconstruct the floor state.
     * May be inaccurate since movement collisions are not considered.
     * If good floor is not found, just keep the old floor and hope it's correct enough.
     */
    switch(d->physstate)
    {
        case PHYS_SLOPE:
        case PHYS_FLOOR:
            d->o.z -= 0.1f;
            if(!collide(d, vec(0, 0, -1), d->physstate == PHYS_SLOPE ? SLOPEZ : FLOORZ))
                d->floor = wall;
            else if(d->physstate == PHYS_SLOPE)
            {
                d->o.z -= d->radius;
                if(!collide(d, vec(0, 0, -1), SLOPEZ))
                    d->floor = wall;
            }
            break;

        case PHYS_STEP_UP:
            d->o.z -= STAIRHEIGHT+0.1f;
            if(!collide(d, vec(0, 0, -1), SLOPEZ))
                d->floor = wall;
            break;

        case PHYS_SLIDE:
            d->o.z -= d->radius+0.1f;
            if(!collide(d, vec(0, 0, -1)) && wall.z < SLOPEZ)
                d->floor = wall;
            break;
    };
    if(d->physstate > PHYS_FALL && d->floor.z <= 0) d->floor = vec(0, 0, 1);
    d->o = old;
};

