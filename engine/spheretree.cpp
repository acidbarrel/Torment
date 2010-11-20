#include "pch.h"
#include "engine.h"

struct SphereBranch : SphereTree
{
    SphereTree *child1, *child2;

    SphereBranch(SphereTree *c1, SphereTree *c2) : child1(c1), child2(c2)
    {
        vec n(child2->center);
        n.sub(child1->center);
        float dist = n.magnitude();

        radius = (child1->radius + child2->radius + dist) / 2;

        center = child1->center;
        if(dist)
        {
            n.mul((radius - child1->radius) / dist);
            center.add(n);
        };
    };

    ~SphereBranch()
    {
        DELETEP(child1);
        DELETEP(child2);
    };

    bool childintersect(const vec &o, const vec &ray, float maxdist, float &dist) const
    {
        return child1->intersect(o, ray, maxdist, dist) ||
               child2->intersect(o, ray, maxdist, dist);
    };
};  

static inline bool raytriintersect(const vec &o, const vec &ray, float maxdist, const triangle &tri, float &dist)
{
    vec edge1(tri.b), edge2(tri.c);
    edge1.sub(tri.a);
    edge2.sub(tri.a);
    vec p;
    p.cross(ray, edge2);
    float det = edge1.dot(p);
    if(det == 0) return false;
    vec r(o);
    r.sub(tri.a);
    float u = r.dot(p) / det;
    if(u < 0 || u > 1) return false;
    vec q;
    q.cross(r, edge1);
    float v = ray.dot(q) / det;
    if(v < 0 || u + v > 1) return false;
    float f = edge2.dot(q) / det;
    if(f < 0 || f > maxdist) return false;
    dist = f;
    return true;
};

struct SphereLeaf : SphereTree
{
    triangle tri;

    SphereLeaf(const triangle &t) : tri(t)
    {
        center = t.a;
        center.add(t.b);
        center.add(t.c);
        center.div(3);
        float r1 = center.dist(t.a), 
              r2 = center.dist(t.b), 
              r3 = center.dist(t.c);
        radius = max(r1, max(r2, r3));
    };
    
    bool childintersect(const vec &o, const vec &ray, float maxdist, float &dist) const
    {
        return raytriintersect(o, ray, maxdist, tri, dist);
    };

    bool isleaf() { return true; };
};

SphereTree *buildspheretree(int numtris, const triangle *tris)
{
    if(numtris<=0) return NULL;

    SphereTree **spheres = new SphereTree *[numtris];
    loopi(numtris) spheres[i] = new SphereLeaf(tris[i]);

    vec center = vec(0, 0, 0);
    loopi(numtris) center.add(spheres[i]->center);
    center.div(numtris);

    int numspheres = numtris;
    while(numspheres>1)
    {
        int farthest = -1;
        float dist = -1e16f;
        loopi(numspheres)
        {
            float d = center.dist(spheres[i]->center) - spheres[i]->radius;
            if(d>dist)
            {
                farthest = i;
                dist = d;
            };
        }; 
        SphereTree *child1 = spheres[farthest];
        int closest = -1;
        float radius = 1e16f;
        loopi(numspheres)
        {
            if(i==farthest) continue;
            SphereTree *child2 = spheres[i];
            float xyradius = (child1->radius + child2->radius + child1->center.dist(child2->center)) / 2;
            if(!xyradius && child1->isleaf() && child2->isleaf() && ((SphereLeaf *)child1)->tri == ((SphereLeaf *)child2)->tri)
            {
                spheres[i] = spheres[--numspheres];
                if(farthest==numspheres) farthest = i;
                else i--;
                continue;
            }    
            else if(xyradius < radius)
            {
                closest = i;
                radius = xyradius;
            };
        };
        if(closest>=0)
        {
            spheres[farthest] = new SphereBranch(spheres[farthest], spheres[closest]);
            spheres[closest] = spheres[--numspheres];
        };
    };
    
    SphereTree *root = spheres[0];
    delete[] spheres;
    return root;
};

static inline void yawray(vec &o, vec &ray, float angle)
{
    angle *= RAD;
    float c = cosf(angle), s = sinf(angle),
          ox = o.x, oy = o.y,
          rx = ox+ray.x, ry = oy+ray.y;
    o.x = ox*c - oy*s;
    o.y = oy*c + ox*s;
    ray.x = rx*c - ry*s - o.x;
    ray.y = ry*c + rx*s - o.y;
    ray.normalize();
};

bool mmintersect(const extentity &e, const vec &o, const vec &ray, float maxdist, int mode, float &dist)
{
    model *m = loadmodel(NULL, e.attr2);
    if(!m) return false;
    if(mode&RAY_SHADOW && (!m->shadow || checktriggertype(e.attr3, TRIG_COLLIDE|TRIG_DISAPPEAR))) return false;
    if(!m->spheretree && !m->setspheretree()) return false;
    if(!maxdist) maxdist = 1e16f;
    vec yo(o);
    yo.sub(e.o);
    if(!m->spheretree->shellintersect(yo, ray, maxdist)) return false;
    float yaw = -180.0f-(float)((e.attr1+7)-(e.attr1+7)%15);
    vec yray(ray);
    if(yaw != 0) yawray(yo, yray, yaw);
    return m->spheretree->childintersect(yo, yray, maxdist, dist);
};

