#include "pch.h"
#include "engine.h"

void boxs(int orient, int x, int y, int xs, int ys, int z)
{
    int d = dimension(orient);
    float f = (dimcoord(orient)>0 ? 0.2f : -0.2f);
    vec m(ivec(d, x,    y,    z).v);
    vec n(ivec(d, x+xs, y,    z).v);
    vec o(ivec(d, x+xs, y+ys, z).v);
    vec p(ivec(d, x,    y+ys, z).v);

    extern int outline;
    if(outline)
    {
        m[d] += f;
        n[d] += f;
        o[d] += f;
        p[d] += f;
    };

    glBegin(GL_POLYGON);

    glVertex3f(m.x, m.y, m.z);
    glVertex3f(n.x, n.y, n.z);
    glVertex3f(o.x, o.y, o.z);
    glVertex3f(p.x, p.y, p.z);

    glEnd();
};

void boxs3D(const ivec &o, const ivec &s, int g)
{
    loopi(6)
    {
        int d = dimension(i), dc = dimcoord(i);
        boxs(i, o[R[d]], o[C[d]], g*s[R[d]], g*s[C[d]], o[d]+dc*g*s[d]);
    };
};

void boxsgrid(const ivec &o, const ivec &s, int g, int orient)
{
    int d = dimension(orient), dc = dimcoord(orient);
    loop(x, s[R[d]]) loop(y, s[C[d]])
        boxs(orient, o[R[d]]+x*g, o[C[d]]+y*g, g, g, o[d]+dc*g*s[d]);
};

selinfo sel = { 0 }, lastsel;

int orient = 0;
int gridsize = 8;
ivec cor, lastcor;
ivec cur, lastcur;

bool editmode = false;
bool havesel = false;

int *hmap = NULL; // heightmap
ushort *htex = NULL; // textures for heightmap
ushort htexture = 0; // single texture for heightmap

extern int entmoving;
extern void entdrag(const vec &ray, int d, ivec &dest, bool first);

VARF(dragging, 0, 0, 1,
    if(!dragging || cor[0]<0) return;
    lastcur = cur;
    lastcor = cor;
    sel.grid = gridsize;
    sel.orient = orient;
);

VAR(selectcorners, 0, 0, 1);

VARF(moving, 0, 0, 1,
    if(!moving) return;
    vec v(cur.v); v.add(1);
    moving = pointinsel(sel, v);
    if(moving) havesel = false; // tell cursorupdate to create handle
);

void clearheighttexture()
{
    if(htex != NULL) delete[] htex;
    htex = NULL;
};

void clearheightmap()
{
    clearheighttexture();
    if(hmap != NULL) delete[] hmap;
    hmap = NULL;
};

void forcenextundo() { lastsel.orient = -1; };

void cubecancel()
{
    havesel = false;
    moving = dragging = 0;
    clearheightmap();
    forcenextundo();
};

void entcancel()
{
    entgroup.setsize(0);
};

void cancelsel()
{
    cubecancel();
    entcancel();
};

VARF(gridpower, 2, 3, VVEC_INT-1,
{
    if(dragging) return;
    gridsize = 1<<gridpower;
    if(gridsize>=hdr.worldsize) gridsize = hdr.worldsize/2;
    cancelsel();
});


VAR(passthroughcube, 0, 0, 1);
VAR(passthroughsel, 0, 0, 1);
VAR(editing,0,0,1);

void toggleedit()
{
    if(player->state!=CS_ALIVE && player->state!=CS_EDITING) return; // do not allow dead players to edit to avoid state confusion
    if(!editmode && !cc->allowedittoggle()) return;         // not in most multiplayer modes
    if(!(editmode = !editmode))
    {
        player->state = CS_ALIVE;
        player->o.z -= player->eyeheight;       // entinmap wants feet pos
        entinmap(player);                       // find spawn closest to current floating pos
    }
    else
    {
        cl->resetgamestate();
        player->state = CS_EDITING;
    };
    cancelsel();
    keyrepeat(editmode);
    editing = editmode;
};

bool noedit(bool view)
{
    if(!editmode) { conoutf("operation only allowed in edit mode"); return true; };
    if(view || haveselent()) return false;
    float r = 1.0f;
    vec o, s;
    o = sel.o.v;
    s = sel.s.v;
    s.mul(float(sel.grid) / 2.0f);
    o.add(s);
    r = float(max(s.x, max(s.y, s.z)));
    bool viewable = (isvisiblesphere(r, o) != VFC_NOT_VISIBLE);
    if(!viewable) conoutf("selection not in view");
    return !viewable;
};

extern void createheightmap();

void reorient()
{
    sel.cx = 0;
    sel.cy = 0;
    sel.cxs = sel.s[R[dimension(orient)]]*2;
    sel.cys = sel.s[C[dimension(orient)]]*2;
    sel.orient = orient;
    if(hmap) createheightmap();
};

void selextend()
{
    if(noedit(true)) return;    
    loopi(3)
    {
        if(cur[i]<sel.o[i])
        {
            sel.s[i] += (sel.o[i]-cur[i])/sel.grid;
            sel.o[i] = cur[i];
        }
        else if(cur[i]>=sel.o[i]+sel.s[i]*sel.grid)
        {
            sel.s[i] = (cur[i]-sel.o[i])/sel.grid+1;
        };
    };
};

COMMANDN(edittoggle, toggleedit, "");
COMMAND(entcancel, "");
COMMAND(cubecancel, "");
COMMAND(cancelsel, "");
COMMAND(reorient, "");
COMMAND(selextend, "");

///////// selection support /////////////

cube &blockcube(int x, int y, int z, const block3 &b, int rgrid) // looks up a world cube, based on coordinates mapped by the block
{
    ivec s(dimension(b.orient), x*b.grid, y*b.grid, dimcoord(b.orient)*(b.s[dimension(b.orient)]-1)*b.grid);

    return neighbourcube(b.o.x+s.x, b.o.y+s.y, b.o.z+s.z, -z*b.grid, rgrid, b.orient);
};

#define loopxy(b)        loop(y,(b).s[C[dimension((b).orient)]]) loop(x,(b).s[R[dimension((b).orient)]])
#define loopxyz(b, r, f) { loop(z,(b).s[D[dimension((b).orient)]]) loopxy((b)) { cube &c = blockcube(x,y,z,b,r); f; }; }
#define loopselxyz(f)    { makeundo(); loopxyz(sel, sel.grid, f); changed(sel); }
#define selcube(x, y, z) blockcube(x, y, z, sel, sel.grid)

////////////// cursor ///////////////

int selchildcount=0;
ivec origin(0,0,0);

ICOMMAND(havesel, "", intret(havesel ? selchildcount : 0));

void countselchild(cube *c, ivec &cor, int size)
{
    ivec ss(sel.s);
    ss.mul(sel.grid);
    loopoctabox(cor, size, sel.o, ss)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].children) countselchild(c[i].children, o, size/2);
        else selchildcount++;
    };
};

void normalizelookupcube(int x, int y, int z)
{
    if(lusize>gridsize)
    {
        lu.x += (x-lu.x)/gridsize*gridsize;
        lu.y += (y-lu.y)/gridsize*gridsize;
        lu.z += (z-lu.z)/gridsize*gridsize;
    }
    else if(gridsize>lusize)
    {
        lu.x &= ~(gridsize-1);
        lu.y &= ~(gridsize-1);
        lu.z &= ~(gridsize-1);
    };
    lusize = gridsize;
};

void updateselection()
{
    sel.o.x = min(lastcur.x, cur.x);
    sel.o.y = min(lastcur.y, cur.y);
    sel.o.z = min(lastcur.z, cur.z);
    sel.s.x = abs(lastcur.x-cur.x)/sel.grid+1;
    sel.s.y = abs(lastcur.y-cur.y)/sel.grid+1;
    sel.s.z = abs(lastcur.z-cur.z)/sel.grid+1;
};

void editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first)
{
    plane pl(d, off);
    float dist = 0.0f;

    if(pl.rayintersect(player->o, ray, dist))
    {        
        dest = ray;
        dest.mul(dist);
        dest.add(player->o);
        if(first)
        {
            handle = dest;
            handle.sub(o);
        };
        dest.sub(handle);
    };
};

void cursorupdate()
{
    if(sel.grid == 0) sel.grid = gridsize;

    vec target(worldpos);
    if(!insideworld(target)) loopi(3) 
        target[i] = max(min(target[i], hdr.worldsize), 0);
    vec ray(target);
    ray.sub(player->o).normalize();
    int g2 = gridsize/2;
    int d = dimension(sel.orient),
        od = dimension(orient),
        odc= dimcoord(orient);
    ivec e;
               
    if(moving)
    {       
        static vec v, handle;
        editmoveplane(sel.o.tovec(), ray, od, sel.o[D[od]]+odc*sel.grid*sel.s[D[od]], handle, v, !havesel);
        if(!havesel)
        {
            v.add(handle);
            (e = handle).mask(~(sel.grid-1));
            v.sub(handle = e.v);
            havesel = true;
        };
        (e = v).mask(~(sel.grid-1));
        sel.o[R[od]] = e[R[od]];
        sel.o[C[od]] = e[C[od]];
    }
    else 
    if(entmoving)
    {
        entdrag(ray, d, e, false);
    }
    else
    {  
        vec v;
        ivec w;
        float sdist = 0, wdist = 0, t;
        bool hit = false;

        wdist = raycubepos(player->o, ray, v, 0,  (editmode && showmat ? RAY_EDITMAT : 0)   // select cubes first
                                                | (passthroughcube ? RAY_PASS : 0)
                                                | RAY_SKIPFIRST, gridsize);
        if(dragging) // update selection 
        {
            w = v;
            lookupcube(w.x, w.y, w.z);
            normalizelookupcube(w.x, w.y, w.z);
            cur = lu;
            updateselection();
        };

        if(havesel && !passthroughsel)     // now try selecting the selection
            if(hit = rayrectintersect(sel.o, ivec(sel.s).mul(sel.grid), player->o, ray, sdist, orient))
                wdist = min(sdist, wdist);  // and choose the nearest of the two

        v = ray;
        v.mul(wdist+0.1f);
        v.add(player->o);
        w = v;
        lookupcube(w.x, w.y, w.z);
        int mag = lusize / gridsize;
        normalizelookupcube(w.x, w.y, w.z);
        if(!hit) rayrectintersect(lu, ivec(gridsize,gridsize,gridsize), player->o, ray, t=0, orient); // just getting orient     
        cur = lu;
        cor = w;
        cor.div(g2);
        od = dimension(orient);
        d = dimension(sel.orient);

        if(dragging) 
        {
            updateselection();
            sel.cx   = min(cor[R[d]], lastcor[R[d]]);
            sel.cy   = min(cor[C[d]], lastcor[C[d]]);
            sel.cxs  = max(cor[R[d]], lastcor[R[d]]);
            sel.cys  = max(cor[C[d]], lastcor[C[d]]);

            if(!selectcorners)
            {
                sel.cx &= ~1;
                sel.cy &= ~1;
                sel.cxs &= ~1;
                sel.cys &= ~1;
                sel.cxs -= sel.cx-2;
                sel.cys -= sel.cy-2;
            }
            else
            {
                sel.cxs -= sel.cx-1;
                sel.cys -= sel.cy-1;
            };

            sel.cx  &= 1;
            sel.cy  &= 1;
            havesel = true;
        }
        else if(!havesel && hmap == NULL)
        {
            sel.o = lu;
            sel.s.x = sel.s.y = sel.s.z = 1;
            sel.cx = sel.cy = 0;
            sel.cxs = sel.cys = 2;
            sel.grid = gridsize;
            sel.orient = orient;
            d = od;
        };

        sel.corner = (cor[R[d]]-lu[R[d]]/g2)+(cor[C[d]]-lu[C[d]]/g2)*2;
        selchildcount = 0;
        countselchild(worldroot, origin, hdr.worldsize/2);
        if(mag>1 && selchildcount==1) selchildcount = -mag;
    };
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    
    // cursors    
    if(entmoving)
    {
        glColor3ub(40,40,40);
        loop(x, 4) loop(y, 4)
            boxs(sel.orient, e[R[d]]+(x-2)*sel.grid, e[C[d]]+(y-2)*sel.grid, sel.grid, sel.grid, e[d]+dimcoord(opposite(sel.orient))*sel.us(d));
    };

    if(!moving)
    {
        glColor3ub(120,120,120);
        boxs(orient, lu[R[od]], lu[C[od]], lusize, lusize, lu[od]+dimcoord(orient)*lusize);
    };

    // selections
    if(hmap != NULL)
    {
        glColor3ub(0,200,0);
        d = dimension(sel.orient);
        loop(x, 2) loop(y, 2) // corners
            boxs(sel.orient, sel.o[R[d]]+x*(sel.us(R[d])-sel.grid), sel.o[C[d]]+y*(sel.us(C[d])-sel.grid), sel.grid, sel.grid, sel.o[d]+dimcoord(sel.orient)*sel.us(d));
        boxs3D(sel.o, sel.s, sel.grid);
    }
    else if(havesel)
    {
        d = dimension(sel.orient);
        glColor3ub(20,20,20);   // grid
        boxsgrid(sel.o, sel.s, sel.grid, sel.orient);
        glColor3ub(200,0,0);    // 0 reference
        boxs(sel.orient, sel.o[R[d]]-4, sel.o[C[d]]-4, 8, 8, sel.o[d]);
        glColor3ub(200,200,200);// 2D selection box
        boxs(sel.orient, sel.o[R[d]]+sel.cx*g2, sel.o[C[d]]+sel.cy*g2, sel.cxs*g2, sel.cys*g2, sel.o[d]+dimcoord(sel.orient)*sel.us(d));
        glColor3ub(0,0,120);     // 3D selection box
        boxs3D(sel.o, sel.s, sel.grid);
    };
    
    glDisable(GL_BLEND);
};

//////////// ready changes to vertex arrays ////////////

void readychanges(block3 &b, cube *c, ivec &cor, int size)
{
    loopoctabox(cor, size, b.o, b.s)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        if(c[i].ext)
        {
            if(c[i].ext->va)             // removes va s so that octarender will recreate
            {
                int hasmerges = c[i].ext->va->hasmerges;
                destroyva(c[i].ext->va);
                c[i].ext->va = NULL;
                if(hasmerges) invalidatemerges(c[i]);
            };
            freeoctaentities(c[i]);
        };
        if(c[i].children)
        {
            if(size<=4)
            {
                solidfaces(c[i]);
                discardchildren(c[i]);
                brightencube(c[i]);
            }
            else readychanges(b, c[i].children, o, size/2);
        }
        else brightencube(c[i]);
    };
};

void changed(const block3 &sel)
{
    block3 b = sel;
    loopi(3) b.s[i] *= b.grid;
    b.grid = 1;
    loopi(3)                    // the changed blocks are the selected cubes
    {
        b.o[i] -= 1;
        b.s[i] += 2;
        readychanges(b, worldroot, origin, hdr.worldsize/2);
        b.o[i] += 1;
        b.s[i] -= 2;
    };

    inbetweenframes = false;
    octarender();
    inbetweenframes = true;
    setupmaterials();
    entitiesinoctanodes();
};

//////////// copy and undo /////////////
cube copycube(cube &src)
{
    cube c = src;
    c.ext = NULL; // src cube is responsible for va destruction
    if(src.children)
    {
        c.children = newcubes(F_EMPTY);
        loopi(8) c.children[i] = copycube(src.children[i]);
    }
    else if(src.ext && src.ext->material!=MAT_AIR) ext(c).material = src.ext->material;
    return c;
};

void pastecube(cube &src, cube &dest)
{
    discardchildren(dest);
    dest = copycube(src);
};

block3 *blockcopy(const block3 &s, int rgrid)
{
    block3 *b = (block3 *)new uchar[sizeof(block3)+sizeof(cube)*s.size()];
    *b = s;
    cube *q = b->c();
    loopxyz(s, rgrid, *q++ = copycube(c));
    return b;
};

void freeblock(block3 *b)
{
    cube *q = b->c();
    loopi(b->size()) discardchildren(*q++);
    delete[] b;
};

int *selgridmap(selinfo &sel)                           // generates a map of the cube sizes at each grid point
{
    int *g = new int[sel.size()];
    loopxyz(sel, -sel.grid, (*g++ = lusize, c));
    return g-sel.size();
};

vector<undoblock> undos;                                // unlimited undo
vector<undoblock> redos;
VARP(undomegs, 0, 5, 100);                              // bounded by n megs

void freeundo(undoblock u)
{
    if(u.g) delete[] u.g;
    if(u.b) freeblock(u.b);
    if(u.e) delete[] u.e;
};

void pasteundo(undoblock &u)
{
    if(u.g)
    {
        int *g = u.g;
        cube *s = u.b->c();
        loopxyz(*u.b, *g++, pastecube(*s++, c));
    };
    pasteundoents(u);
};

void pruneundos(int maxremain)                          // bound memory
{
    int t = 0, p = 0;
    loopvrev(undos)
    {
        undoblock &u = undos[i];
        if(u.b)
        {
            cube *q = u.b->c();
            t += u.b->size()*sizeof(int);
            loopj(u.b->size())
                t += familysize(*q++)*sizeof(cube);
        };
        t += u.n*sizeof(undoent);
        if(t>maxremain) freeundo(undos.remove(i)); else p = t;
    };
    //conoutf("undo: %d of %d(%%%d)", p, undomegs<<20, p*100/(undomegs<<20));
    while(!redos.empty()) { freeundo(redos.pop()); };
};

void initundocube(undoblock &u, selinfo &sel)
{
    u.g = selgridmap(sel);
    u.b = blockcopy(sel, -sel.grid);
};

void addundo(undoblock &u)
{
    undos.add(u);
    pruneundos(undomegs<<20);
};

bool undogoahead = false;

void makeundo()                        // stores state of selected cubes before editing
{
    undogoahead = false;
    if(lastsel==sel) return;
    lastsel=sel;
    if(multiplayer(false)) return;
    undogoahead = true;
    undoblock u;
    initundocube(u, sel);
    addundo(u);
};

void swapundo(vector<undoblock> &a, vector<undoblock> &b, const char *s)
{
    if(noedit() || multiplayer()) return;
    if(a.empty()) { conoutf("nothing more to %s", s); return; };
    undoblock u = a.pop();
    if(u.b)
    {
        sel.o = u.b->o;
        sel.s = u.b->s;
        sel.grid = u.b->grid;
        sel.orient = u.b->orient;
    };
    undoblock r;
    if(u.g) initundocube(r, sel);
    if(u.n) copyundoents(r, u);
    b.add(r);
    pasteundo(u);
    if(u.b) changed(sel);
    freeundo(u);
    clearheightmap();
    reorient();
    forcenextundo();
};

void editundo() { swapundo(undos, redos, "undo"); };
void editredo() { swapundo(redos, undos, "redo"); };

editinfo *localedit=NULL;

void freeeditinfo(editinfo *&e)
{
    if(!e) return;
    if(e->copy) freeblock(e->copy);
    delete e;
    e = NULL;
};

// guard against subdivision
#define protectsel(f) { undoblock _u; initundocube(_u, sel); f; pasteundo(_u); freeundo(_u); }

void mpcopy(editinfo *&e, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_COPY);
    if(e==NULL) e = new editinfo;
    if(e->copy) freeblock(e->copy);
    e->copy = NULL;
    protectsel(e->copy = blockcopy(block3(sel), sel.grid));
    changed(sel);
};

void mppaste(editinfo *&e, selinfo &sel, bool local)
{
    if(e==NULL) return;
    if(local) cl->edittrigger(sel, EDIT_PASTE);
    if(e->copy)
    {
        sel.s = e->copy->s;
        int o = sel.orient;
        sel.orient = e->copy->orient;
        cube *s = e->copy->c();
        loopselxyz(pastecube(*s++, c));
        sel.orient = o;
    };
};

void copy()
{
    if(noedit(true)) return;
    mpcopy(localedit, sel, true);
};

void paste(int *isdown)
{
    if(noedit()) return;
    if(*isdown!=0)
    {
        if(localedit && localedit->copy)
        {
            sel.s = localedit->copy->s;
            havesel = true;
            reorient();
        };
    }
    else if(havesel)
        mppaste(localedit, sel, true);
};

COMMAND(copy, "");
COMMAND(paste, "D");
COMMANDN(undo, editundo, "");
COMMANDN(redo, editredo, "");

///////////// height maps ////////////////

void pushside(cube &c, int d, int x, int y, int z)
{
    ivec a;
    getcubevector(c, d, x, y, z, a);
    a[R[d]] = 8 - a[R[d]];
    setcubevector(c, d, x, y, z, a);
};

void setheightmap(selinfo &b)
{
    int d = dimension(sel.orient);
    int dc = dimcoord(sel.orient);
    int w = sel.s[R[d]] + 1;
    int h = (dc ? sel.s[D[d]] : 0) + (dc ? sel.o[D[d]] : hdr.worldsize - sel.o[D[d]]) / sel.grid;

    makeundo();

    b.o[R[d]]  = max(sel.o[R[d]], b.o[R[d]]-sel.grid);
    b.o[C[d]]  = max(sel.o[C[d]], b.o[C[d]]-sel.grid);
    int sx     = (b.o[R[d]]-sel.o[R[d]])/sel.grid;
    int sy     = (b.o[C[d]]-sel.o[C[d]])/sel.grid;
    b.s[R[d]]  = max(0, min(b.s[R[d]]-sx+2, sel.s[R[d]]-sx));
    b.s[C[d]]  = max(0, min(b.s[C[d]]-sy+2, sel.s[C[d]]-sy));

    loopxyz(b, b.grid,

        if(c.children)
            discardchildren(c);
        solidfaces(c);

        loopi(2) loopj(2)
        {
            int e = min(8, hmap[x+sx+i+(y+sy+j)*w] - (h-z-1)*8);
            if(e<0)
            {
                e=0;
                pushside(c, d, i, j, 0);
                pushside(c, d, i, j, 1);
            };
            edgeset(cubeedge(c, d, i, j), dc, dc ? e : 8-e);
        };

        c.texture[sel.orient] = (htex ? htex[x+sx+(y+sy)*w] : htexture);
        optiface((uchar *)&c.faces[d], c);
    );

    changed(b);
};

void cubifyheightmap(selinfo &b)     // pull up heighfields to where they don't cross cube boundaries
{
    int d = dimension(sel.orient);
    int w = sel.s[R[d]] + 1;
    int l = sel.s[C[d]] + 1;
    for(;;)
    {
        bool changed = false;
        loop(x, w-1)
        {
            loop(y, l-1)
            {
                int *o[4];
                loopi(2) loopj(2) o[i+j*2] = hmap+x+i+(y+j)*w;
                int best = 0xFFFF;
                loopi(4) if(*o[i]<best) best = *o[i];
                int bottom = (best&(~7))+8;
                if((*o[0]==*o[3] && *o[0]==bottom) ||
                   (*o[1]==*o[2] && *o[1]==bottom)) bottom += 8;
                loopj(4) if(*o[j]>bottom)
                {
                    *o[j] = bottom;
                    changed = true;
                    b.o[R[d]] = min(b.o[R[d]], x*sel.grid+sel.o[R[d]]);
                    b.o[C[d]] = min(b.o[C[d]], y*sel.grid+sel.o[C[d]]);
                    b.s[R[d]] = max(b.s[R[d]], x+1);
                    b.s[C[d]] = max(b.s[C[d]], y+1);
                };
            };
        };
        if(!changed) break;
    };
};

void getlimits(int &d, int &dc, int &w, int &l, int &lo, int &hi, int &himax)
{
    d = dimension(sel.orient);
    dc = dimcoord(sel.orient);
    w = sel.s[R[d]] + 1;
    l = sel.s[C[d]] + 1;
    lo = 8 * (sel.o[D[d]] / sel.grid);
    hi = 8 * (sel.s[D[d]]) + lo;
    himax = 8 * hdr.worldsize / sel.grid;

    if(!dc)
    {
        swap(int, hi, lo);
        hi = himax - hi;
        lo = himax - lo;
    };
};

void createheightmap()
{
    int d, dc, w, l, lo, hi, himax;
    getlimits(d, dc, w, l, lo, hi, himax);

    clearheightmap();
    hmap = new int[w*l];
    htex = new ushort[w*l];
    loop(x, w) loop(y, l)
    {
        hmap[x+y*w] = lo;
        htex[x+y*w] = 0;
    };

    int h = hi / 8;
    selinfo b(sel);
    makeundo();
    loopxyz(b, b.grid,
        if(c.children) { solidfaces(c); discardchildren(c); };
        if(!htex[x+y*w] && z == sel.s[D[d]]-1) htex[x+y*w] = c.texture[sel.orient];
        if(isempty(c)) continue;
        if(!htex[x+y*w]) htex[x+y*w] = c.texture[sel.orient];

        loopi(2) loopj(2)
        {
            int a = x+i+(y+j)*w;
            int e = edgeget(cubeedge(c, d, i, j), dc);
            e = (h-z-1)*8 + (dc ? e : 8-e);
            hmap[a] = max(hmap[a], e);// simply take the heighest points
        };
    );
    cubifyheightmap(b);
    setheightmap(b);
};

void getheightmap()
{
    if(noedit() || multiplayer() || haveselent()) return;
    createheightmap();
};

COMMAND(getheightmap, "");

const int MAXBRUSH = 50;
int brush[MAXBRUSH][MAXBRUSH];
VAR(brushx, 0, 25, MAXBRUSH);
VAR(brushy, 0, 25, MAXBRUSH);
int brushmaxx = 0;
int brushmaxy = 0;

void clearbrush()
{
    loopi(MAXBRUSH) loopj(MAXBRUSH)
        brush[i][j] = 0;
    brushmaxx = 0;
    brushmaxy = 0;
 };

void brushvert(int *x, int *y, int *v)
{
    if(*x<0 || *y<0 || *x>=MAXBRUSH || *y>=MAXBRUSH) return;
    brush[*x][*y] = *v;
    brushmaxx = max(brushmaxx, *x+1);
    brushmaxy = max(brushmaxy, *y+1);
};

int getxcursor() { int d = dimension(sel.orient); return (cur[R[d]] - sel.o[R[d]]) / sel.grid + (sel.corner&1 ? 1 : 0); };
int getycursor() { int d = dimension(sel.orient); return (cur[C[d]] - sel.o[C[d]]) / sel.grid + (sel.corner&2 ? 1 : 0); };

void copybrush()
{
    if(hmap == NULL) return;
    int d, dc, w, l, lo, hi, himax; getlimits(d, dc, w, l, lo, hi, himax);
    if(w>=MAXBRUSH || l>=MAXBRUSH) return conoutf("Selection is too big to generate brush");
    clearbrush();
    loop(x, w) loop(y, l)
        brush[x][y] = hi - hmap[x+y*w];
    brushx = max(0, min(MAXBRUSH, getxcursor()));
    brushy = max(0, min(MAXBRUSH, getycursor()));
};

void savebrush(const char *name)
{
    FILE *f = fopen("mybrushes.cfg", "a");
    if(!f) return;
    fprintf(f, "newbrush \"%s\" %d %d [\n", name, brushx, brushy);
    int skipped = 0;
    loop(y, MAXBRUSH)
    {
        int last = 0;
        loop(x, MAXBRUSH) if(brush[x][y]!=0) last = x+1;
        if(!last) { skipped++; continue; };
        while(skipped) { fprintf(f, "\"\"\n"); skipped--; };
        fprintf(f, "\"");
        loop(x, last)
            fprintf(f, "%d ", brush[x][y]);
        fprintf(f, "\"\n");
    };
    fprintf(f, "]\n\n");
    conoutf("Brush \"%s\" saved", name);
    fclose(f);
};

COMMAND(clearbrush, "");
COMMAND(brushvert, "iii");
COMMAND(copybrush, "");
COMMAND(savebrush, "s");

void edithmap(int dir)
{
    if(multiplayer() || hmap == NULL) return;

    int d, dc, w, l, lo, hi, himax;
    getlimits(d, dc, w, l, lo, hi, himax);
    int x = getxcursor();
    int y = getycursor();

    if(x<0 || y<0 || x>=w || y>=l) return;

    int bx = x>brushx ? 0 : brushx - x;
    int by = y>brushy ? 0 : brushy - y;
    int sx = x>brushx ? brushx : x;
    int sy = y>brushy ? brushy : y;
    int ex = min(brushmaxx - brushx, w - x);
    int ey = min(brushmaxy - brushy, l - y);

    loopi(sx+ex) loopj(sy+ey)
    {
        int index = x+i-sx+(y+j-sy)*w;
        hmap[index] -= brush[i+bx][j+by]*dir;
        hmap[index] = max(0, min(hmap[index], himax));
        while(hmap[index] > hi) { hi += 8; sel.s[D[d]] += 1; if(!dc) sel.o[D[d]] -= sel.grid; };
        while(hmap[index] < lo) { lo -= 8; sel.s[D[d]] += 1; if(dc)  sel.o[D[d]] -= sel.grid; };
    };

    selinfo b;
    b.o[D[d]] = sel.o[D[d]];
    b.s[D[d]] = sel.s[D[d]];
    b.o[C[d]] = sel.o[C[d]]+(y-sy)*sel.grid;
    b.o[R[d]] = sel.o[R[d]]+(x-sx)*sel.grid;
    b.s[C[d]] = y+ey;
    b.s[R[d]] = x+ex;
    b.grid   = sel.grid;
    b.orient = sel.orient;
    cubifyheightmap(b);
    setheightmap(b);
};

///////////// main cube edit ////////////////

int bounded(int n) { return n<0 ? 0 : (n>8 ? 8 : n); };

void pushedge(uchar &edge, int dir, int dc)
{
    int ne = bounded(edgeget(edge, dc)+dir);
    edge = edgeset(edge, dc, ne);
    int oe = edgeget(edge, 1-dc);
    if((dir<0 && dc && oe>ne) || (dir>0 && dc==0 && oe<ne)) edge = edgeset(edge, 1-dc, ne);
};

void linkedpush(cube &c, int d, int x, int y, int dc, int dir)
{
    ivec v, p;
    getcubevector(c, d, x, y, dc, v);

    loopi(2) loopj(2)
    {
        getcubevector(c, d, i, j, dc, p);
        if(v==p)
            pushedge(cubeedge(c, d, i, j), dir, dc);
    };
};

static uchar getmaterial(cube &c)
{
    if(c.children)
    {
        uchar mat = getmaterial(c.children[7]);
        loopi(7) if(mat != getmaterial(c.children[i])) return MAT_AIR;
        return mat;
    };
    return c.ext ? c.ext->material : MAT_AIR;
};

VAR(invalidcubeguard, 0, 1, 1);

void mpeditface(int dir, int mode, selinfo &sel, bool local)
{
    if(mode==1 && (sel.cx || sel.cy || sel.cxs&1 || sel.cys&1)) mode = 0;
    int d = dimension(sel.orient);
    int dc = dimcoord(sel.orient);
    int seldir = dc ? -dir : dir;

    if(local)
        cl->edittrigger(sel, EDIT_FACE, dir, mode);

    if(mode==1)
    {
        int h = sel.o[d]+dc*sel.grid;
        if((dir>0 == dc && h<=0) || (dir<0 == dc && h>=hdr.worldsize)) return;
        if(dir<0) sel.o[d] += sel.grid * seldir;
    };

    if(dc) sel.o[d] += sel.us(d)-sel.grid;
    sel.s[d] = 1;

    loopselxyz(
        if(c.children) solidfaces(c);
        uchar mat = getmaterial(c);
        discardchildren(c);
        if(mat!=MAT_AIR) ext(c).material = mat;
        if(mode==1) // fill command
        {
            if(dir<0)
            {
                solidfaces(c);
                cube &o = blockcube(x, y, 1, sel, -sel.grid);
                loopi(6)
                    c.texture[i] = o.texture[i];
            }
            else
                emptyfaces(c);
        }
        else
        {
            uint bak = c.faces[d];
            uchar *p = (uchar *)&c.faces[d];

            if(mode==2)
                linkedpush(c, d, sel.corner&1, sel.corner>>1, dc, seldir); // corner command
            else
            {
                loop(mx,2) loop(my,2)                                       // pull/push edges command
                {
                    if(x==0 && mx==0 && sel.cx) continue;
                    if(y==0 && my==0 && sel.cy) continue;
                    if(x==sel.s[R[d]]-1 && mx==1 && (sel.cx+sel.cxs)&1) continue;
                    if(y==sel.s[C[d]]-1 && my==1 && (sel.cy+sel.cys)&1) continue;
                    if(p[mx+my*2] != ((uchar *)&bak)[mx+my*2]) continue;

                    linkedpush(c, d, mx, my, dc, seldir);
                };
            };

            optiface(p, c);
            if(invalidcubeguard==1 && !isvalidcube(c))
            {
                uint newbak = c.faces[d];
                uchar *m = (uchar *)&bak;
                uchar *n = (uchar *)&newbak;
                loopk(4) if(n[k] != m[k]) // tries to find partial edit that is valid
                {
                    c.faces[d] = bak;
                    c.edges[d*4+k] = n[k];
                    if(isvalidcube(c))
                        m[k] = n[k];
                };
                c.faces[d] = bak;
            };
        };
    );
    if (mode==1 && dir>0)
        sel.o[d] += sel.grid * seldir;
};

void editface(int *dir, int *mode)
{
    if(noedit(moving!=0)) return;
    if(hmap)
        edithmap(*dir);
    else
        mpeditface(*dir, *mode, sel, true);
};

void pushsel(int *dir)
{
    if(noedit(moving!=0)) return;
    int d = dimension(orient);
    int s = dimcoord(orient) ? -*dir : *dir;
    sel.o[d] += s*sel.grid;
};

void mpdelcube(selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_DELCUBE);
    loopselxyz(discardchildren(c); emptyfaces(c));
};

void delcube() 
{
    if(noedit()) return;
    mpdelcube(sel, true);
};

COMMAND(pushsel, "i");
COMMAND(editface, "ii");
COMMAND(delcube, "");

/////////// texture editing //////////////////

int curtexindex = -1, lasttex = 0;
int texpaneltimer = 0;

void tofronttex()                                       // maintain most recently used of the texture lists when applying texture
{
    int c = curtexindex;
    if(c>=0)
    {
        texmru.insert(0, texmru.remove(c));
        curtexindex = -1;
    };
};

selinfo repsel;
int reptex = -1;

void edittexcube(cube &c, int tex, int orient, bool &findrep)
{
    if(orient<0) loopi(6) c.texture[i] = tex;
    else
    {
        int i = visibleorient(c, orient);
        if(findrep)
        {
            if(reptex < 0) reptex = c.texture[i];
            else if(reptex != c.texture[i]) findrep = false;
        };
        c.texture[i] = tex;
    };
    if(c.children) loopi(8) edittexcube(c.children[i], tex, orient, findrep);
};

extern int curtexnum;
VAR(allfaces, 0, 0, 1);

void mpedittex(int tex, int allfaces, selinfo &sel, bool local)
{
    if(local)
    {
        cl->edittrigger(sel, EDIT_TEX, tex, allfaces);
        if(allfaces || !(repsel == sel)) reptex = -1;
        repsel = sel;
    };
    bool findrep = local && !allfaces && reptex < 0;
    loopselxyz(edittexcube(c, tex, allfaces ? -1 : sel.orient, findrep));
};

void filltexlist()
{
    if(texmru.length()!=curtexnum)
    {
        loopv(texmru) if(texmru[i]>=curtexnum) texmru.remove(i--);
        loopi(curtexnum) if(texmru.find(i)<0) texmru.add(i);
    };
};

void edittex(int *dir)
{
    if(noedit()) return;
    filltexlist();
    texpaneltimer = 5000;
    if(!(lastsel==sel)) tofronttex();
    int i = curtexindex;
    i = i<0 ? 0 : i+*dir;
    curtexindex = i = min(max(i, 0), curtexnum-1);
    int t = lasttex = htexture = texmru[i];
    clearheighttexture();
    mpedittex(t, allfaces, sel, true);
};

void gettex()
{
    if(noedit()) return;
    filltexlist();
    loopxyz(sel, sel.grid, curtexindex = c.texture[sel.orient]);
    loopi(curtexnum) if(texmru[i]==curtexindex)
    {
        curtexindex = i;
        tofronttex();
        return;
    };
};

COMMAND(edittex, "i");
COMMAND(gettex, "");

void replacetexcube(cube &c, int oldtex, int newtex)
{
    loopi(6) if(c.texture[i] == oldtex) c.texture[i] = newtex;
    if(c.children) loopi(8) replacetexcube(c.children[i], oldtex, newtex);
};

void mpreplacetex(int oldtex, int newtex, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_REPLACE, oldtex, newtex);
    loopi(8) replacetexcube(worldroot[i], oldtex, newtex);
    allchanged();
};

void replace()
{
    if(noedit()) return;
    if(reptex < 0) { conoutf("can only replace after a texture edit"); return; };
    mpreplacetex(reptex, lasttex, sel, true);
};

COMMAND(replace, "");

////////// flip and rotate ///////////////
uint dflip(uint face) { return face==F_EMPTY ? face : 0x88888888 - (((face&0xF0F0F0F0)>>4)+ ((face&0x0F0F0F0F)<<4)); };
uint cflip(uint face) { return ((face&0xFF00FF00)>>8) + ((face&0x00FF00FF)<<8); };
uint rflip(uint face) { return ((face&0xFFFF0000)>>16)+ ((face&0x0000FFFF)<<16); };
uint mflip(uint face) { return (face&0xFF0000FF) + ((face&0x00FF0000)>>8) + ((face&0x0000FF00)<<8); };

void flipcube(cube &c, int d)
{
    swap(ushort, c.texture[d*2], c.texture[d*2+1]);
    c.faces[D[d]] = dflip(c.faces[D[d]]);
    c.faces[C[d]] = cflip(c.faces[C[d]]);
    c.faces[R[d]] = rflip(c.faces[R[d]]);
    if (c.children)
    {
        loopi(8) if (i&octadim(d)) swap(cube, c.children[i], c.children[i-octadim(d)]);
        loopi(8) flipcube(c.children[i], d);
    };
};

void rotatequad(cube &a, cube &b, cube &c, cube &d)
{
    cube t = a; a = b; b = c; c = d; d = t;
};

void rotatecube(cube &c, int d)   // rotates cube clockwise. see pics in cvs for help.
{
    c.faces[D[d]] = cflip (mflip(c.faces[D[d]]));
    c.faces[C[d]] = dflip (mflip(c.faces[C[d]]));
    c.faces[R[d]] = rflip (mflip(c.faces[R[d]]));
    swap(uint, c.faces[R[d]], c.faces[C[d]]);

    swap(uint, c.texture[2*R[d]], c.texture[2*C[d]+1]);
    swap(uint, c.texture[2*C[d]], c.texture[2*R[d]+1]);
    swap(uint, c.texture[2*C[d]], c.texture[2*C[d]+1]);

    if(c.children)
    {
        int row = octadim(R[d]);
        int col = octadim(C[d]);
        for(int i=0; i<=octadim(d); i+=octadim(d)) rotatequad
        (
            c.children[i+row],
            c.children[i],
            c.children[i+col],
            c.children[i+col+row]
        );
        loopi(8) rotatecube(c.children[i], d);
    };
};

void mpflip(selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_FLIP);
    int zs = sel.s[dimension(sel.orient)];
    makeundo();
    loopxy(sel)
    {
        loop(z,zs) flipcube(selcube(x, y, z), dimension(sel.orient));
        loop(z,zs/2)
        {
            cube &a = selcube(x, y, z);
            cube &b = selcube(x, y, zs-z-1);
            swap(cube, a, b);
        };
    };
    changed(sel);
};

void flip()
{
    if(noedit() || hmap!=NULL) return;
    mpflip(sel, true);
};

void mprotate(int cw, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_ROTATE, cw);
    int d = dimension(sel.orient);
    if(!dimcoord(sel.orient)) cw = -cw;
    int &m = min(sel.s[C[d]], sel.s[R[d]]);
    int ss = m = max(sel.s[R[d]], sel.s[C[d]]);
    makeundo();
    loop(z,sel.s[D[d]]) loopi(cw>0 ? 1 : 3)
    {
        loopxy(sel) rotatecube(selcube(x,y,z), d);
        loop(y,ss/2) loop(x,ss-1-y*2) rotatequad
        (
            selcube(ss-1-y, x+y, z),
            selcube(x+y, y, z),
            selcube(y, ss-1-x-y, z),
            selcube(ss-1-x-y, ss-1-y, z)
        );
    };
    changed(sel);
};

void rotate(int *cw)
{
    if(noedit() || hmap!=NULL) return;
    mprotate(*cw, sel, true);
};

COMMAND(flip, "");
COMMAND(rotate, "i");

void setmat(cube &c, uchar mat)
{
    if(c.children)
        loopi(8) setmat(c.children[i], mat);
    else if(mat!=MAT_AIR) ext(c).material = mat;
    else if(c.ext) c.ext->material = MAT_AIR;
};

void mpeditmat(int matid, selinfo &sel, bool local)
{
    if(local) cl->edittrigger(sel, EDIT_MAT, matid);
    loopselxyz(setmat(c, matid));
};

void editmat(char *name)
{
    if(noedit()) return;
    int id = findmaterial(name);
    if(id<0) { conoutf("unknown material \"%s\"", name); return; };
    mpeditmat(id, sel, true);
};

COMMAND(editmat, "s");

void render_texture_panel(int w, int h)
{
    if((texpaneltimer -= curtime)>0 && editmode)
    {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glLoadIdentity();
        int width = w*1800/h;
        glOrtho(0, width, 1800, 0, -1, 1);
        int y = 50, gap = 10;

        static Shader *rgbonlyshader = NULL;
        if(!rgbonlyshader) rgbonlyshader = lookupshaderbyname("rgbonly");
        
        rgbonlyshader->set();

        loopi(7)
        {
            int s = (i == 3 ? 285 : 220), ti = curtexindex+i-3;
            if(ti>=0 && ti<curtexnum)
            {
                Texture *tex = lookuptexture(texmru[ti]).sts[0].t;
                float sx = min(1, tex->xs/(float)tex->ys), sy = min(1, tex->ys/(float)tex->xs);
                glBindTexture(GL_TEXTURE_2D, tex->gl);
                glColor4f(0, 0, 0, texpaneltimer/1000.0f);
                int x = width-s-50, r = s;
                loopj(2)
                {
                    glBegin(GL_QUADS);
                    glTexCoord2f(0.0,    0.0);    glVertex2f(x,   y);
                    glTexCoord2f(1.0/sx, 0.0);    glVertex2f(x+r, y);
                    glTexCoord2f(1.0/sx, 1.0/sy); glVertex2f(x+r, y+r);
                    glTexCoord2f(0.0,    1.0/sy); glVertex2f(x,   y+r);
                    glEnd();
                    xtraverts += 4;
                    glColor4f(1.0, 1.0, 1.0, texpaneltimer/1000.0f);
                    r -= 10;
                    x += 5;
                    y += 5;
                };
            };
            y += s+gap;
        };
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    };
};
