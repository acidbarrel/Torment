// rendercubes.cpp: sits in between worldrender.cpp and rendergl.cpp and fills the vertex array for different cube surfaces.

#include "pch.h"
#include "engine.h"

vector<vertex> verts;

struct cstat { int size, nleaf, nnode, nface; } cstats[32];

VAR(showcstats, 0, 0, 1);

void printcstats()
{
    if(showcstats) loopi(32)
    {
        if(!cstats[i].size) continue;
        conoutf("%d: %d faces, %d leafs, %d nodes", cstats[i].size, cstats[i].nface, cstats[i].nleaf, cstats[i].nnode);
    };
};

VARF(floatvtx, 0, 0, 1, allchanged());

void genfloatverts(fvertex *f)
{
    loopv(verts)
    {
        const vertex &v = verts[i];
        f->x = v.x;
        f->y = v.y;
        f->z = v.z;
        f->u = v.u;
        f->v = v.v;
        f->n = v.n;
        f++;
    };
};

struct vboinfo
{
    int uses;
};

static inline uint hthash(GLuint key)
{
    return key;
};

static inline bool htcmp(GLuint x, GLuint y)
{
    return x==y;
};

hashtable<GLuint, vboinfo> vbos;

VAR(printvbo, 0, 0, 1);
VARF(vbosize, 0, 0, 1024*1024, allchanged());

enum
{
    VBO_VBUF = 0,
    VBO_EBUF_L0,
    VBO_EBUF_L1,
    VBO_SKYBUF_L0,
    VBO_SKYBUF_L1,
    NUMVBO
};

static vector<char> vbodata[NUMVBO];
static vector<vtxarray *> vbovas[NUMVBO];

void destroyvbo(GLuint vbo)
{
    vboinfo &vbi = vbos[vbo];
    if(vbi.uses <= 0) return;
    vbi.uses--;
    if(!vbi.uses) glDeleteBuffers_(1, &vbo);
};

void genvbo(int type, void *buf, int len, vtxarray **vas, int numva)
{
    GLuint vbo;
    glGenBuffers_(1, &vbo);
    GLenum target = type==VBO_VBUF ? GL_ARRAY_BUFFER_ARB : GL_ELEMENT_ARRAY_BUFFER_ARB;
    glBindBuffer_(target, vbo);
    glBufferData_(target, len, buf, GL_STATIC_DRAW_ARB);
    glBindBuffer_(target, 0);
    
    vboinfo &vbi = vbos[vbo]; 
    vbi.uses = numva;
    
    if(printvbo) conoutf("vbo %d: type %d, size %d, %d uses", vbo, type, len, numva);

    loopi(numva)
    {
        vtxarray *va = vas[i];
        switch(type)
        {
            case VBO_VBUF: va->vbufGL = vbo; break;
            case VBO_EBUF_L0: va->l0.ebufGL = vbo; break;
            case VBO_EBUF_L1: va->l1.ebufGL = vbo; break;
            case VBO_SKYBUF_L0: va->l0.skybufGL = vbo; break;
            case VBO_SKYBUF_L1: va->l1.skybufGL = vbo; break;
        };
    };
};

void flushvbo(int type = -1)
{
    if(type < 0)
    {
        loopi(NUMVBO) flushvbo(i);
        return;
    };

    vector<char> &data = vbodata[type];
    if(data.empty()) return;
    vector<vtxarray *> &vas = vbovas[type];
    genvbo(type, data.getbuf(), data.length(), vas.getbuf(), vas.length());
    data.setsizenodelete(0);
    vas.setsizenodelete(0);
};

void *addvbo(vtxarray *va, int type, void *buf, int len)
{
    int minsize = type==VBO_VBUF ? min(vbosize, int(floatvtx ? sizeof(fvertex) : sizeof(vertex)) << 16) : vbosize;

    if(len >= minsize)
    {
        genvbo(type, buf, len, &va, 1);
        return 0;
    };

    vector<char> &data = vbodata[type];
    vector<vtxarray *> &vas = vbovas[type];

    if(data.length() && data.length() + len > minsize) flushvbo(type);

    data.reserve(len);

    size_t offset = data.length();
    data.reserve(len);
    memcpy(&data.getbuf()[offset], buf, len);
    data.ulen += len;

    vas.add(va); 

    if(data.length() >= minsize) flushvbo(type);

    return (void *)offset;
};
 
struct vechash
{
    static const int size = 1<<16;
    int table[size];
    vector<int> chain;

    vechash() { clear(); };
    void clear() { loopi(size) table[i] = -1; chain.setsizenodelete(0); };

    int access(const vvec &v, short tu, short tv, const bvec &n)
    {
        const uchar *iv = (const uchar *)&v;
        uint h = 5381;
        loopl(sizeof(v)) h = ((h<<5)+h)^iv[l];
        h = h&(size-1);
        for(int i = table[h]; i>=0; i = chain[i])
        {
            const vertex &c = verts[i];
            if(c.x==v.x && c.y==v.y && c.z==v.z && c.n==n)
            {
                 if(!tu && !tv) return i; 
                 if(c.u==tu && c.v==tv) return i;
            };
        };
        vertex &vtx = verts.add();
        ((vvec &)vtx) = v;
        vtx.u = tu;
        vtx.v = tv;
        vtx.n = n;
        chain.add(table[h]);
        return table[h] = verts.length()-1;
    };
};

vechash vh;

struct sortkey
{
     uint tex, lmid;
     sortkey() {};
     sortkey(uint tex, uint lmid)
      : tex(tex), lmid(lmid)
     {};

     bool operator==(const sortkey &o) const { return tex==o.tex && lmid==o.lmid; };
};

struct sortval
{
     int unlit;
     usvector dims[3];

     sortval() : unlit(0) {};
};

static inline bool htcmp(const sortkey &x, const sortkey &y)
{
    return x.tex == y.tex && x.lmid == y.lmid;
};

static inline uint hthash(const sortkey &k)
{
    return k.tex + k.lmid*9741;
};

struct lodcollect
{
    hashtable<sortkey, sortval> indices;
    vector<sortkey> texs;
    vector<materialsurface> matsurfs;
    usvector skyindices, explicitskyindices;
    int curtris;
    uint offsetindices;

    int size() { return texs.length()*sizeof(elementset) + (hasVBO ? 0 : (3*curtris+skyindices.length()+explicitskyindices.length())*sizeof(ushort)) + matsurfs.length()*sizeof(materialsurface); };

    void clearidx() { indices.clear(); };
    void clear()
    {
        curtris = 0;
        offsetindices = 0;
        skyindices.setsizenodelete(0);
        explicitskyindices.setsizenodelete(0);
        matsurfs.setsizenodelete(0);
    };

    void remapunlit(vector<sortkey> &unlit)
    {
        uint lastlmid = LMID_AMBIENT, firstlmid = LMID_AMBIENT;
        int firstlit = -1;
        loopv(texs)
        {
            sortkey &k = texs[i];
            if(k.lmid>=LMID_RESERVED) 
            {
                lastlmid = lightmaps[k.lmid-LMID_RESERVED].unlitx>=0 ? k.lmid : LMID_AMBIENT;
                if(firstlit<0)
                {
                    firstlit = i;
                    firstlmid = lastlmid;
                };
            }
            else if(k.lmid==LMID_AMBIENT && lastlmid!=LMID_AMBIENT)
            {
                sortval &t = indices[k];
                if(t.unlit<=0) t.unlit = lastlmid;
            };
        };
        if(firstlmid!=LMID_AMBIENT) loopi(firstlit)
        {
            sortkey &k = texs[i];
            if(k.lmid!=LMID_AMBIENT) continue;
            indices[k].unlit = firstlmid;
        }; 
        loopv(unlit)
        {
            sortkey &k = unlit[i];
            sortval &t = indices[k];
            if(t.unlit<=0) continue; 
            LightMap &lm = lightmaps[t.unlit-LMID_RESERVED];
            short u = short((lm.unlitx + 0.5f) * SHRT_MAX/LM_PACKW), 
                  v = short((lm.unlity + 0.5f) * SHRT_MAX/LM_PACKH);
            loopl(3) loopvj(t.dims[l])
            {
                vertex &vtx = verts[t.dims[l][j]];
                if(!vtx.u && !vtx.v)
                {
                    vtx.u = u;
                    vtx.v = v;
                }
                else if(vtx.u != u || vtx.v != v) 
                {
                    // necessary to copy these in case vechash reallocates verts before copying vtx
                    vvec vv = vtx;
                    bvec n = vtx.n;
                    t.dims[l][j] = vh.access(vv, u, v, n);
                };
            };
            sortval *dst = indices.access(sortkey(k.tex, t.unlit));
            if(dst) loopl(3) loopvj(t.dims[l]) dst->dims[l].add(t.dims[l][j]);
        };
    };
                    
    void optimize()
    {
        vector<sortkey> unlit;

        texs.setsizenodelete(0);
        enumeratekt(indices, sortkey, k, sortval, t,
            loopl(3) if(t.dims[l].length() && t.unlit<=0)
            {
                if(k.lmid>=LMID_RESERVED && lightmaps[k.lmid-LMID_RESERVED].unlitx>=0)
                {
                    sortkey ukey(k.tex, LMID_AMBIENT);
                    sortval *uval = indices.access(ukey);
                    if(uval && uval->unlit<=0)
                    {
                        if(uval->unlit<0) texs.removeobj(ukey);
                        else unlit.add(ukey);
                        uval->unlit = k.lmid;
                    };
                }
                else if(k.lmid==LMID_AMBIENT)
                {
                    unlit.add(k);
                    t.unlit = -1;
                };
                texs.add(k);
                break;
            };
        );
        texs.sort(texsort);

        remapunlit(unlit);

        matsurfs.setsize(optimizematsurfs(matsurfs.getbuf(), matsurfs.length()));
    };

    static int texsort(const sortkey *x, const sortkey *y)
    {
        if(x->tex == y->tex) return 0;
        Slot &xs = lookuptexture(x->tex, false), &ys = lookuptexture(y->tex, false);
        if(xs.shader < ys.shader) return -1;
        if(xs.shader > ys.shader) return 1;
        if(xs.params.length() < ys.params.length()) return -1;
        if(xs.params.length() > ys.params.length()) return 1;
        if(x->tex < y->tex) return -1;
        else return 1;
    };

    char *setup(vtxarray *va, lodlevel &lod, char *buf)
    {
        lod.eslist = (elementset *)buf;

        lod.sky = skyindices.length();
        lod.explicitsky = explicitskyindices.length();

        if(!hasVBO)
        {
            lod.ebufGL = lod.skybufGL = 0;
            lod.ebuf = (ushort *)(lod.eslist + texs.length());
            lod.skybuf = lod.ebuf + 3*curtris;
            lod.matbuf = (materialsurface *)(lod.skybuf+lod.sky+lod.explicitsky);
        };

        ushort *skybuf = NULL;
        if(lod.sky+lod.explicitsky)
        {
            skybuf = hasVBO ? new ushort[lod.sky+lod.explicitsky] : lod.skybuf;
            memcpy(skybuf, skyindices.getbuf(), lod.sky*sizeof(ushort));
            memcpy(skybuf+lod.sky, explicitskyindices.getbuf(), lod.explicitsky*sizeof(ushort));
        };
 
        if(hasVBO)
        {
            lod.ebuf = lod.skybuf = 0;
            lod.matbuf = (materialsurface *)(lod.eslist + texs.length());

            if(skybuf)
            {
#if 1
                if(offsetindices) loopi(lod.sky+lod.explicitsky) skybuf[i] += offsetindices;
                lod.skybuf = (ushort *)addvbo(va, &lod==&va->l0 ? VBO_SKYBUF_L0 : VBO_SKYBUF_L1, skybuf, (lod.sky+lod.explicitsky)*sizeof(ushort));
#else
                glGenBuffers_(1, &lod.skybufGL);
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, lod.skybufGL);
                glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, (lod.sky+lod.explicitsky)*sizeof(ushort), skybuf, GL_STATIC_DRAW_ARB);
#endif
                delete[] skybuf;
            }
            else lod.skybufGL = 0;
        };

        lod.matsurfs = matsurfs.length();
        if(lod.matsurfs) memcpy(lod.matbuf, matsurfs.getbuf(), matsurfs.length()*sizeof(materialsurface));

        if(texs.length())
        {
            ushort *ebuf = hasVBO ? new ushort[3*curtris] : lod.ebuf, *curbuf = ebuf;
            loopv(texs)
            {
                const sortkey &k = texs[i];
                const sortval &t = indices[k];
                lod.eslist[i].texture = k.tex;
                lod.eslist[i].lmid = t.unlit>0 ? t.unlit : k.lmid;
                loopl(3) if((lod.eslist[i].length[l] = t.dims[l].length()))
                {
                    memcpy(curbuf, t.dims[l].getbuf(), t.dims[l].length() * sizeof(ushort));
                    curbuf += t.dims[l].length();
                };
            };
            if(hasVBO)
            {
#if 1
                if(offsetindices) loopi(3*curtris) ebuf[i] += offsetindices;
                lod.ebuf = (ushort *)addvbo(va, &lod==&va->l0 ? VBO_EBUF_L0 : VBO_EBUF_L1, ebuf, 3*curtris*sizeof(ushort));
#else
                glGenBuffers_(1, &lod.ebufGL);
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, lod.ebufGL);
                glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, 3*curtris*sizeof(ushort), ebuf, GL_STATIC_DRAW_ARB);
#endif
                delete[] ebuf;
            };
        }
        else if(hasVBO) lod.ebufGL = 0;
        lod.texs = texs.length();
        lod.tris = curtris;
        return (char *)(lod.matbuf+lod.matsurfs);
    };
} l0, l1;

int explicitsky = 0, skyarea = 0;

VARF(lodsize, 0, 32, 128, hdr.mapwlod = lodsize);
VAR(loddistance, 0, 2000, 100000);

int addtriindexes(usvector &v, int index[4])
{
    int tris = 0;
    if(index[0]!=index[1] && index[0]!=index[2] && index[1]!=index[2])
    {
        tris++;
        v.add(index[0]);
        v.add(index[1]);
        v.add(index[2]);
    };
    if(index[0]!=index[2] && index[0]!=index[3] && index[2]!=index[3])
    {
        tris++;
        v.add(index[0]);
        v.add(index[2]);
        v.add(index[3]);
    };
    return tris;
};

void addcubeverts(int orient, int size, bool lodcube, vvec *vv, ushort texture, surfaceinfo *surface, surfacenormals *normals)
{
    int index[4];
    loopk(4)
    {
        short u, v;
        if(surface && surface->lmid >= LMID_RESERVED)
        {
            u = short((surface->x + (surface->texcoords[k*2] / 255.0f) * (surface->w - 1) + 0.5f) * SHRT_MAX/LM_PACKW);
            v = short((surface->y + (surface->texcoords[k*2 + 1] / 255.0f) * (surface->h - 1) + 0.5f) * SHRT_MAX/LM_PACKH);
        }
        else u = v = 0;
        index[k] = vh.access(vv[k], u, v, normals ? normals->normals[k] : bvec(128, 128, 128));
    };

    extern vector<GLuint> lmtexids;
    sortkey key(texture, surface && lmtexids.inrange(surface->lmid) ? surface->lmid : LMID_AMBIENT);
    if(!lodcube)
    {
        int tris = addtriindexes(texture == DEFAULT_SKY ? l0.explicitskyindices : l0.indices[key].dims[dimension(orient)], index);
        if(texture == DEFAULT_SKY) explicitsky += tris;
        else l0.curtris += tris;
    };
    if(lodsize && size>=lodsize)
    {
        int tris = addtriindexes(texture == DEFAULT_SKY ? l1.explicitskyindices : l1.indices[key].dims[dimension(orient)], index);
        if(texture != DEFAULT_SKY) l1.curtris += tris;
    };
};

void gencubeverts(cube &c, int x, int y, int z, int size, int csi, bool lodcube)
{
    freeclipplanes(c);                          // physics planes based on rendering

    loopi(6) if(visibleface(c, i, x, y, z, size, MAT_AIR, lodcube))
    {
        cubeext &e = ext(c);

        // this is necessary for physics to work, even if the face is merged
        if(touchingface(c, i)) e.visible |= 1<<i;

        if(!lodcube && e.merged&(1<<i)) continue;

        cstats[csi].nface++;

        vvec vv[4];
        loopk(4) calcvert(c, x, y, z, size, vv[k], faceverts(c, i, k));
        addcubeverts(i, size, lodcube, vv, c.texture[i], e.surfaces ? &e.surfaces[i] : NULL, e.normals ? &e.normals[i] : NULL);
    };
};

bool skyoccluded(cube &c, int orient)
{
    if(isempty(c)) return false;
//    if(c.texture[orient] == DEFAULT_SKY) return true;
    if(touchingface(c, orient) && faceedges(c, orient) == F_SOLID) return true;
    return false;
};

int hasskyfaces(cube &c, int x, int y, int z, int size, int faces[6])
{
    int numfaces = 0;
    if(x == 0 && !skyoccluded(c, O_LEFT)) faces[numfaces++] = O_LEFT;
    if(x + size == hdr.worldsize && !skyoccluded(c, O_RIGHT)) faces[numfaces++] = O_RIGHT;
    if(y == 0 && !skyoccluded(c, O_BACK)) faces[numfaces++] = O_BACK;
    if(y + size == hdr.worldsize && !skyoccluded(c, O_FRONT)) faces[numfaces++] = O_FRONT;
    if(z == 0 && !skyoccluded(c, O_BOTTOM)) faces[numfaces++] = O_BOTTOM;
    if(z + size == hdr.worldsize && !skyoccluded(c, O_TOP)) faces[numfaces++] = O_TOP;
    return numfaces;
};

vector<cubeface> skyfaces[6][2];
 
void minskyface(cube &cu, int orient, const ivec &co, int size, mergeinfo &orig)
{   
    mergeinfo mincf;
    mincf.u1 = orig.u2;
    mincf.u2 = orig.u1;
    mincf.v1 = orig.v2;
    mincf.v2 = orig.v1;
    mincubeface(cu, orient, co, size, orig, mincf);
    orig.u1 = max(mincf.u1, orig.u1);
    orig.u2 = min(mincf.u2, orig.u2);
    orig.v1 = max(mincf.v1, orig.v1);
    orig.v2 = min(mincf.v2, orig.v2);
};  

void genskyfaces(cube &c, const ivec &o, int size, bool lodcube)
{
    if(isentirelysolid(c)) return;

    int faces[6],
        numfaces = hasskyfaces(c, o.x, o.y, o.z, size, faces);
    if(!numfaces) return;

    loopi(numfaces)
    {
        int orient = faces[i], dim = dimension(orient);
        cubeface m;
        m.c = NULL;
        m.u1 = (o[C[dim]]&VVEC_INT_MASK)<<VVEC_FRAC; 
        m.u2 = ((o[C[dim]]&VVEC_INT_MASK)+size)<<VVEC_FRAC;
        m.v1 = (o[R[dim]]&VVEC_INT_MASK)<<VVEC_FRAC;
        m.v2 = ((o[R[dim]]&VVEC_INT_MASK)+size)<<VVEC_FRAC;
        minskyface(c, orient, o, size, m);
        if(m.u1 >= m.u2 || m.v1 >= m.v2) continue;
        if(!lodcube) 
        {
            skyarea += (int(m.u2-m.u1)*int(m.v2-m.v1) + (1<<(2*VVEC_FRAC))-1)>>(2*VVEC_FRAC);
            skyfaces[orient][0].add(m);
        };
        if(lodsize && size>=lodsize) skyfaces[orient][1].add(m);
    };
};

void addskyverts(const ivec &o, int size)
{
    loopi(6)
    {
        int dim = dimension(i), c = C[dim], r = R[dim];
        loopl(2)
        {
            vector<cubeface> &sf = skyfaces[i][l]; 
            if(sf.empty()) continue;
            sf.setsizenodelete(mergefaces(i, sf.getbuf(), sf.length()));
            loopvj(sf)
            {
                mergeinfo &m = sf[j];
                int index[4];
                loopk(4)
                {
                    const ivec &coords = cubecoords[fv[i][3-k]];
                    vvec vv;
                    vv[dim] = (o[dim]&VVEC_INT_MASK)<<VVEC_FRAC;
                    if(coords[dim]) vv[dim] += size<<VVEC_FRAC;
                    vv[c] = coords[c] ? m.u2 : m.u1;
                    vv[r] = coords[r] ? m.v2 : m.v1;
                    index[k] = vh.access(vv, 0, 0, bvec(128, 128, 128));
                };
                addtriindexes((!l ? l0 : l1).skyindices, index);
            };
            sf.setsizenodelete(0);
        };
    };
};
                    
////////// Vertex Arrays //////////////

int allocva = 0;
int wtris = 0, wverts = 0, vtris = 0, vverts = 0, glde = 0;
vector<vtxarray *> valist, varoot;

vtxarray *newva(int x, int y, int z, int size)
{
    l0.optimize();
    l1.optimize();
    int allocsize = sizeof(vtxarray) + l0.size() + l1.size();
    int bufsize = verts.length()*(floatvtx ? sizeof(fvertex) : sizeof(vertex));
    if(!hasVBO) allocsize += bufsize; // length of vertex buffer
    vtxarray *va = (vtxarray *)new uchar[allocsize];
    if(hasVBO && verts.length())
    {
        void *vbuf;
        if(floatvtx)
        {
            fvertex *f = new fvertex[verts.length()];
            genfloatverts(f);
            vbuf = (vertex *)addvbo(va, VBO_VBUF, f, bufsize); 
            delete[] f;
        }
        else vbuf = (vertex *)addvbo(va, VBO_VBUF, verts.getbuf(), bufsize);
        int offset = int(size_t(vbuf)) / (floatvtx ? sizeof(fvertex) : sizeof(vertex)); 
        l0.offsetindices = offset;
        l1.offsetindices = offset;
        va->vbuf = 0; // Offset in VBO
    };
    char *buf = l1.setup(va, va->l1, l0.setup(va, va->l0, (char *)(va+1)));
    if(!hasVBO)
    {
        va->vbufGL = 0;
        va->vbuf = (vertex *)buf;
        if(floatvtx) genfloatverts((fvertex *)buf);
        else memcpy(va->vbuf, verts.getbuf(), bufsize);
    };

    va->parent = NULL;
    va->children = new vector<vtxarray *>;
    va->allocsize = allocsize;
    va->x = x; va->y = y; va->z = z; va->size = size;
    va->explicitsky = explicitsky;
    va->skyarea = skyarea;
    va->curvfc = VFC_NOT_VISIBLE;
    va->occluded = OCCLUDE_NOTHING;
    va->query = NULL;
    va->mapmodels = NULL;
    va->hasmerges = 0;
    wverts += va->verts = verts.length();
    wtris  += va->l0.tris;
    allocva++;
    valist.add(va);
    return va;
};

void destroyva(vtxarray *va, bool reparent)
{
    if(va->vbufGL) destroyvbo(va->vbufGL);
    if(va->l0.ebufGL) destroyvbo(va->l0.ebufGL);
    if(va->l0.skybufGL) destroyvbo(va->l0.skybufGL);
    if(va->l1.ebufGL) destroyvbo(va->l1.ebufGL);
    if(va->l1.skybufGL) destroyvbo(va->l1.skybufGL);
    wverts -= va->verts;
    wtris -= va->l0.tris;
    allocva--;
    valist.removeobj(va);
    if(!va->parent) varoot.removeobj(va);
    if(reparent)
    {
        if(va->parent) va->parent->children->removeobj(va);
        loopv(*va->children)
        {
            vtxarray *child = (*va->children)[i];
            child->parent = va->parent;
            if(child->parent) child->parent->children->add(va);
        };
    };
    if(va->mapmodels) delete va->mapmodels;
    if(va->children) delete va->children;
    delete[] (uchar *)va;
};

void vaclearc(cube *c)
{
    loopi(8)
    {
        if(c[i].ext)
        {
            if(c[i].ext->va) destroyva(c[i].ext->va, false);
            c[i].ext->va = NULL;
        };
        if(c[i].children) vaclearc(c[i].children);
    };
};

static ivec bbmin, bbmax;
static vector<octaentities *> vamms;

struct mergedface
{   
    mergedface *next;
    uchar orient;
    ushort tex;
    vvec v[4];
    surfaceinfo *surface;
    surfacenormals *normals;
};  

struct mflist
{
    mergedface *first, *last;
    int count;
};

static int vahasmerges = 0, vamergemax = 0;
static mflist vamerges[VVEC_INT];

void genmergedfaces(cube &c, const ivec &co, int size, int minlevel = 0)
{
    if(!c.ext || !c.ext->merges) return;
    int index = 0;
    loopi(6) if(c.ext->mergeorigin & (1<<i))
    {
        mergeinfo &m = c.ext->merges[index++];
        if(m.u1>=m.u2 || m.v1>=m.v2) continue;
        mergedface mf;
        mf.orient = i;
        mf.tex = c.texture[i];
        mf.surface = c.ext->surfaces ? &c.ext->surfaces[i] : NULL;
        mf.normals = c.ext->normals ? &c.ext->normals[i] : NULL;
        genmergedverts(c, i, co, size, m, mf.v);
        int level = calcmergedsize(i, co, size, m, mf.v);
        if(level > minlevel)
        {
            mergedface &nf = *new mergedface;
            nf = mf;
            mflist &mfl = vamerges[level];
            nf.next = mfl.first;
            mfl.first = &nf;
            if(!mfl.last) mfl.last = &nf;
            mfl.count++;
            vamergemax = max(vamergemax, level);
            vahasmerges |= MERGE_ORIGIN;
        };
    };
};

void findmergedfaces(cube &c, const ivec &co, int size, int csi, int minlevel)
{
    if(c.ext && c.ext->va && !(c.ext->va->hasmerges&MERGE_ORIGIN)) return;
    if(c.children)
    {
        loopi(8)
        {
            ivec o(i, co.x, co.y, co.z, size/2); 
            findmergedfaces(c.children[i], o, size/2, csi-1, minlevel);
        };
    }
    else if(c.ext && c.ext->merges) genmergedfaces(c, co, size, minlevel);
};

void addmergedverts(int level)
{
    mflist &mfl = vamerges[level];
    if(!mfl.count) return;
    while(mfl.count)
    {
        mergedface &mf = *mfl.first;
        mfl.first = mf.next;
        if(mfl.last == &mf) mfl.last = NULL;
        addcubeverts(mf.orient, 1<<level, false, mf.v, mf.tex, mf.surface, mf.normals);
        delete &mf;
        mfl.count--;
        cstats[level].nface++;
        vahasmerges |= MERGE_USE;
    };
};

void rendercube(cube &c, int cx, int cy, int cz, int size, int csi)  // creates vertices and indices ready to be put into a va
{
    //if(size<=16) return;
    if(c.ext && c.ext->va) return;                            // don't re-render
    cstats[csi].size = size;
    bool lodcube = false;

    if(c.children)
    {
        cstats[csi].nnode++;

        loopi(8)
        {
            ivec o(i, cx, cy, cz, size/2);
            rendercube(c.children[i], o.x, o.y, o.z, size/2, csi-1);
        };

        if(csi < VVEC_INT && vamerges[csi].count) addmergedverts(csi);

        if(size!=lodsize)
        {
            if(c.ext)
            {
                if(c.ext->ents && c.ext->ents->mapmodels.length()) vamms.add(c.ext->ents);
            };
            return;
        };
        lodcube = true;
    };
    if(!c.children || lodcube) genskyfaces(c, ivec(cx, cy, cz), size, lodcube);

    if(!isempty(c))
    {
        gencubeverts(c, cx, cy, cz, size, csi, lodcube);

        if(cx<bbmin.x) bbmin.x = cx;
        if(cy<bbmin.y) bbmin.y = cy;
        if(cz<bbmin.z) bbmin.z = cz;
        if(cx+size>bbmax.x) bbmax.x = cx+size;
        if(cy+size>bbmax.y) bbmax.y = cy+size;
        if(cz+size>bbmax.z) bbmax.z = cz+size;
    };

    if(lodcube) return;

    if(c.ext)
    {
        if(c.ext->ents && c.ext->ents->mapmodels.length()) vamms.add(c.ext->ents);
        if(c.ext->material != MAT_AIR) genmatsurfs(c, cx, cy, cz, size, l0.matsurfs);
        if(c.ext->merges) genmergedfaces(c, ivec(cx, cy, cz), size);
        if(c.ext->merged & ~c.ext->mergeorigin) vahasmerges |= MERGE_PART;
    };

    if(csi < VVEC_INT && vamerges[csi].count) addmergedverts(csi);

    cstats[csi].nleaf++;
};

void setva(cube &c, int cx, int cy, int cz, int size, int csi)
{
    ASSERT(size <= VVEC_INT_MASK+1);

    if(verts.length())                                 // since reseting is a bit slow
    {
        verts.setsizenodelete(0);
        explicitsky = skyarea = 0;
        vh.clear();
        l0.clear();
        l1.clear();
    };

    vamms.setsizenodelete(0);

    bbmin = ivec(cx+size, cy+size, cz+size);
    bbmax = ivec(cx, cy, cz);

    rendercube(c, cx, cy, cz, size, csi);

    addskyverts(ivec(cx, cy, cz), size);

    if(verts.length())
    {
        vtxarray *va = newva(cx, cy, cz, size);
        ext(c).va = va;
        va->min = bbmin;
        va->max = bbmax;
        if(vamms.length()) va->mapmodels = new vector<octaentities *>(vamms);
        va->hasmerges = vahasmerges;
    };

    l0.clearidx();
    l1.clearidx();
};

VARF(vacubemax, 64, 2048, 256*256, allchanged());
VARF(vacubesize, 128, 128, VVEC_INT_MASK+1, allchanged());
VARF(vacubemin, 0, 128, 256*256, allchanged());

int recalcprogress = 0;
#define progress(s)     if((recalcprogress++&0x7FF)==0) show_out_of_renderloop_progress(recalcprogress/(float)allocnodes, s);

int updateva(cube *c, int cx, int cy, int cz, int size, int csi)
{
    progress("recalculating geometry...");
    static int faces[6];
    int ccount = 0, cmergemax = vamergemax, chasmerges = vahasmerges;
    loopi(8)                                    // counting number of semi-solid/solid children cubes
    {
        int count = 0, childpos = varoot.length();
        ivec o(i, cx, cy, cz, size);
        vamergemax = 0;
        vahasmerges = 0;
        if(c[i].ext && c[i].ext->va) 
        {
            //count += vacubemax+1;       // since must already have more then max cubes
            varoot.add(c[i].ext->va);
            if(c[i].ext->va->hasmerges&MERGE_ORIGIN) findmergedfaces(c[i], o, size, csi, csi);
        }
        else if(c[i].children) count += updateva(c[i].children, o.x, o.y, o.z, size/2, csi-1);
        else if(!isempty(c[i]) || hasskyfaces(c[i], o.x, o.y, o.z, size, faces)) count++;
        int tcount = count + (csi < VVEC_INT ? vamerges[csi].count : 0);
        if(tcount > vacubemax || (tcount >= vacubemin && size == vacubesize) || (tcount && size == min(VVEC_INT_MASK+1, hdr.worldsize/2))) 
        {
            setva(c[i], o.x, o.y, o.z, size, csi);
            if(c[i].ext && c[i].ext->va)
            {
                while(varoot.length() > childpos)
                {
                    vtxarray *child = varoot.pop();
                    c[i].ext->va->children->add(child);
                    child->parent = c[i].ext->va;
                };
                varoot.add(c[i].ext->va);
                if(vamergemax > size)
                {
                    cmergemax = max(cmergemax, vamergemax);
                    vahasmerges |= vahasmerges&~MERGE_USE;
                };
                continue;
            };
        };
        if(csi < VVEC_INT-1 && vamerges[csi].count)
        {
            mflist &mfl = vamerges[csi], &nfl = vamerges[csi+1];
            mfl.last->next = nfl.first;
            nfl.first = mfl.first; 
            if(!nfl.last) nfl.last = mfl.last;
            mfl.first = mfl.last = 0;
            nfl.count += mfl.count;
            mfl.count = 0;
        };
        cmergemax = max(cmergemax, vamergemax);
        chasmerges |= vahasmerges;
        ccount += count;
    };

    vamergemax = cmergemax;
    vahasmerges = chasmerges;

    return ccount;
};

void genlod(cube &c, int size)
{
    if(!c.children || (c.ext && c.ext->va)) return;
    progress("generating LOD...");

    loopi(8) genlod(c.children[i], size/2);

    if(size>lodsize) return;

    if(c.ext) c.ext->material = MAT_AIR;

    loopi(8) if(!isempty(c.children[i]))
    {
        forcemip(c);
        return;
    };

    emptyfaces(c);
};

void octarender()                               // creates va s for all leaf cubes that don't already have them
{
    recalcprogress = 0;
    if(lodsize) loopi(8) genlod(worldroot[i], hdr.worldsize/2);

    int csi = 0;
    while(1<<csi < hdr.worldsize) csi++;

    recalcprogress = 0;
    varoot.setsizenodelete(0);
    updateva(worldroot, 0, 0, 0, hdr.worldsize/2, csi-1);
    flushvbo();

    explicitsky = 0;
    skyarea = 0;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        explicitsky += va->explicitsky;
        skyarea += va->skyarea;
    };
};

void precachetextures(lodlevel &lod) { loopi(lod.texs) lookuptexture(lod.eslist[i].texture); };
void precacheall() { loopv(valist) { precachetextures(valist[i]->l0); precachetextures(valist[i]->l1); } ; };

void allchanged(bool load)
{
    show_out_of_renderloop_progress(0, "clearing VBOs...");
    vaclearc(worldroot);
    memset(cstats, 0, sizeof(cstat)*32);
    resetqueries();
    octarender();
    if(load) precacheall();
    setupmaterials(load);
    printcstats();
};

void recalc()
{
    allchanged(true);
};

COMMAND(recalc, "");

///////// view frustrum culling ///////////////////////

plane vfcP[5];  // perpindictular vectors to view frustrum bounding planes
float vfcDfog;  // far plane culling distance (fog limit).
int vfcw, vfch, vfcfov;

vtxarray *visibleva;

int isvisiblesphere(float rad, const vec &cv)
{
    int v = VFC_FULL_VISIBLE;
    float dist;

    loopi(5)
    {
        dist = vfcP[i].dist(cv);
        if(dist < -rad) return VFC_NOT_VISIBLE;
        if(dist < rad) v = VFC_PART_VISIBLE;
    };

    dist = vfcP[0].dist(cv) - vfcDfog;
    if(dist > rad) return VFC_FOGGED;  //VFC_NOT_VISIBLE;    // culling when fog is closer than size of world results in HOM
    if(dist > -rad) v = VFC_PART_VISIBLE;

    return v;
};

int isvisiblecube(const vec &o, int size)
{
    vec center(o);
    center.add(size/2.0f);
    return isvisiblesphere(size*SQRT3/2.0f, center);
};


float vadist(vtxarray *va, const vec &p)
{
    if(va->min.x>va->max.x)
    {
        ivec o(va->x, va->y, va->z);
        return p.dist_to_bb(o, ivec(o).add(va->size)); // box contains only sky/water
    };
    return p.dist_to_bb(va->min, va->max);
};

#define VASORTSIZE 64

static vtxarray *vasort[VASORTSIZE];

void addvisibleva(vtxarray *va)
{
    float dist = vadist(va, camera1->o);
    va->distance = int(dist); /*cv.dist(camera1->o) - va->size*SQRT3/2*/
    va->curlod   = lodsize==0 || va->distance<loddistance ? 0 : 1;

    int hash = min(int(dist*VASORTSIZE/hdr.worldsize), VASORTSIZE-1);
    vtxarray **prev = &vasort[hash], *cur = vasort[hash];

    while(cur && va->distance > cur->distance)
    {
        prev = &cur->next;
        cur = cur->next;
    };

    va->next = *prev;
    *prev = va;
};

void sortvisiblevas()
{
    visibleva = NULL; 
    vtxarray **last = &visibleva;
    loopi(VASORTSIZE) if(vasort[i])
    {
        vtxarray *va = vasort[i];
        *last = va;
        while(va->next) va = va->next;
        last = &va->next;
    };
};

void findvisiblevas(vector<vtxarray *> &vas, bool resetocclude = false)
{
    loopv(vas)
    {
        vtxarray &v = *vas[i];
        int prevvfc = resetocclude ? VFC_NOT_VISIBLE : v.curvfc;
        v.curvfc = isvisiblecube(vec(v.x, v.y, v.z), v.size);
        if(v.curvfc!=VFC_NOT_VISIBLE) 
        {
            addvisibleva(&v);
            if(v.children->length()) findvisiblevas(*v.children, prevvfc==VFC_NOT_VISIBLE);
            if(prevvfc==VFC_NOT_VISIBLE)
            {
                v.occluded = OCCLUDE_NOTHING;
                v.query = NULL;
            };
        };
    };
};

void setvfcP(float pyaw, float ppitch, const vec &camera)
{
    float vpxo = 90.0 - vfcfov / 2.0;
    float vpyo = 90.0 - (vfcfov * float(vfch) / float(vfcw)) / 2;
    float yaw = pyaw * RAD;
    float yawp = (pyaw + vpxo) * RAD;
    float yawm = (pyaw - vpxo) * RAD;
    float pitch = ppitch * RAD;
    float pitchp = (ppitch + vpyo) * RAD;
    float pitchm = (ppitch - vpyo) * RAD;
    vfcP[0].toplane(vec(yaw,  pitch), camera);  // back/far plane
    vfcP[1].toplane(vec(yawp, pitch), camera);  // left plane
    vfcP[2].toplane(vec(yawm, pitch), camera);  // right plane
    vfcP[3].toplane(vec(yaw,  pitchp), camera); // top plane
    vfcP[4].toplane(vec(yaw,  pitchm), camera); // bottom plane
    vfcDfog = getvar("fog");
};

plane oldvfcP[5];

void reflectvfcP(float z)
{
    memcpy(oldvfcP, vfcP, sizeof(vfcP));

    vec o(camera1->o);
    o.z = z-(camera1->o.z-z);
    setvfcP(camera1->yaw, -camera1->pitch, o);
};

void restorevfcP()
{
    memcpy(vfcP, oldvfcP, sizeof(vfcP));
};

void visiblecubes(cube *c, int size, int cx, int cy, int cz, int w, int h, int fov)
{
    memset(vasort, 0, sizeof(vasort));

    vfcw = w;
    vfch = h;
    vfcfov = fov;

    // Calculate view frustrum: Only changes if resize, but...
    setvfcP(camera1->yaw, camera1->pitch, camera1->o);

    findvisiblevas(varoot);
    sortvisiblevas();
};

bool insideva(const vtxarray *va, const vec &v)
{
    return va->x<=v.x && va->y<=v.y && va->z<=v.z && va->x+va->size>v.x && va->y+va->size>v.y && va->z+va->size>v.z;
};

static ivec vaorigin;

void resetorigin()
{
    vaorigin = ivec(-1, -1, -1);
};

void setorigin(vtxarray *va)
{
    ivec o(va->x, va->y, va->z);
    o.mask(~VVEC_INT_MASK);
    if(o != vaorigin)
    {
        vaorigin = o;
        glPopMatrix();
        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);
        static const float scale = 1.0f/(1<<VVEC_FRAC);
        glScalef(scale, scale, scale);
    };
};

void setupTMU()
{
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT,  GL_MODULATE);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT,  GL_PREVIOUS_EXT);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_EXT, GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT,  GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
};

#define MAXQUERY 2048

struct queryframe
{
    int cur, max;
    occludequery queries[MAXQUERY];
};

static queryframe queryframes[2] = {{0, 0}, {0, 0}};
static uint flipquery = 0;

int getnumqueries()
{
    return queryframes[flipquery].cur;
};

void flipqueries()
{
    flipquery = (flipquery + 1) % 2;
    queryframe &qf = queryframes[flipquery];
    loopi(qf.cur) qf.queries[i].owner = NULL;
    qf.cur = 0;
};

occludequery *newquery(void *owner)
{
    queryframe &qf = queryframes[flipquery];
    if(qf.cur >= qf.max)
    {
        if(qf.max >= MAXQUERY) return NULL;
        glGenQueries_(1, &qf.queries[qf.max++].id);
    };
    occludequery *query = &qf.queries[qf.cur++];
    query->owner = owner;
    query->fragments = -1;
    return query;
};

void resetqueries()
{
    loopi(2) loopj(queryframes[i].max) queryframes[i].queries[j].owner = NULL;
};

VAR(oqfrags, 0, 8, 64);
VAR(oqreflect, 0, 4, 64);

extern float reflecting, refracting;

bool checkquery(occludequery *query, bool nowait)
{
    GLuint fragments;
    if(query->fragments >= 0) fragments = query->fragments;
    else
    {
        if(nowait)
        {
            GLint avail;
            glGetQueryObjectiv_(query->id, GL_QUERY_RESULT_AVAILABLE, &avail);
            if(!avail) return false;
        };
        glGetQueryObjectuiv_(query->id, GL_QUERY_RESULT_ARB, &fragments);
        query->fragments = fragments;
    };
    return fragments < (uint)(reflecting ? oqreflect : oqfrags);
};

void drawbb(const ivec &bo, const ivec &br, const vec &camera = camera1->o)
{
    glBegin(GL_QUADS);

    loopi(6)
    {
        int dim = dimension(i), coord = dimcoord(i);

        if(coord)
        {
            if(camera[dim] < bo[dim] + br[dim]) continue;
        }
        else if(camera[dim] > bo[dim]) continue;

        loopj(4)
        {
            const ivec &cc = cubecoords[fv[i][j]];
            glVertex3i(cc.x ? bo.x+br.x : bo.x,
                       cc.y ? bo.y+br.y : bo.y,
                       cc.z ? bo.z+br.z : bo.z);
        };

        xtraverts += 4;
    };

    glEnd();
};

extern int octaentsize;

static octaentities *visiblemms, **lastvisiblemms;

void findvisiblemms(const vector<extentity *> &ents)
{
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->mapmodels || va->curvfc >= VFC_FOGGED || va->occluded >= OCCLUDE_BB) continue;
        loopv(*va->mapmodels)
        {
            octaentities *oe = (*va->mapmodels)[i];
            if(isvisiblecube(oe->o.tovec(), oe->size) >= VFC_FOGGED) continue;

            bool occluded = oe->query && oe->query->owner == oe && checkquery(oe->query);
            if(occluded)
            {
                oe->distance = -1;

                oe->next = NULL;
                *lastvisiblemms = oe;
                lastvisiblemms = &oe->next;
            }
            else
            {
                int visible = 0;
                loopv(oe->mapmodels)
                {
                    extentity &e = *ents[oe->mapmodels[i]];
                    if(e.visible || (e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED)) continue;
                    e.visible = true;
                    ++visible;
                };
                if(!visible) continue;

                oe->distance = int(camera1->o.dist_to_bb(oe->o, oe->size));

                octaentities **prev = &visiblemms, *cur = visiblemms;
                while(cur && cur->distance >= 0 && oe->distance > cur->distance)
                {
                    prev = &cur->next;
                    cur = cur->next;
                };

                if(*prev == NULL) lastvisiblemms = &oe->next;
                oe->next = *prev;
                *prev = oe;
            };
        };
    };
};

VAR(oqmm, 0, 4, 8);

extern bool getentboundingbox(extentity &e, ivec &o, ivec &r);

void rendermapmodel(extentity &e)
{
    int anim = ANIM_MAPMODEL|ANIM_LOOP, basetime = 0;
    if(e.attr3) switch(e.triggerstate)
    {
        case TRIGGER_RESET: anim = ANIM_TRIGGER|ANIM_START; break;
        case TRIGGERING: anim = ANIM_TRIGGER; basetime = e.lasttrigger; break;
        case TRIGGERED: anim = ANIM_TRIGGER|ANIM_END; break;
        case TRIGGER_RESETTING: anim = ANIM_TRIGGER|ANIM_REVERSE; basetime = e.lasttrigger; break;
    };
    mapmodelinfo &mmi = getmminfo(e.attr2);
    if(&mmi) rendermodel(e.color, e.dir, mmi.name, anim, 0, mmi.tex, e.o.x, e.o.y, e.o.z, (float)((e.attr1+7)-(e.attr1+7)%15), 0, 10.0f, basetime, NULL, MDL_CULL_VFC | MDL_CULL_DIST);
};

extern int reflectdist;

static vector<octaentities *> renderedmms;

vtxarray *reflectedva;

void renderreflectedmapmodels(float z, bool refract)
{
    bool reflected = !refract && camera1->o.z >= z;
    vector<octaentities *> reflectedmms;
    vector<octaentities *> &mms = reflected ? reflectedmms : renderedmms;
    const vector<extentity *> &ents = et->getents();

    if(reflected)
    {
        reflectvfcP(z);
        for(vtxarray *va = reflectedva; va; va = va->rnext)
        {
            if(!va->mapmodels || va->distance > reflectdist) continue;
            loopv(*va->mapmodels) reflectedmms.add((*va->mapmodels)[i]);
        };
    };
    loopv(mms)
    {
        octaentities *oe = mms[i];
        if(refract ? oe->o.z >= z : oe->o.z+oe->size <= z) continue;
        if(reflected && isvisiblecube(oe->o.tovec(), oe->size) >= VFC_FOGGED) continue;
        loopv(oe->mapmodels)
        {
           extentity &e = *ents[oe->mapmodels[i]];
           if(e.visible || (e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED)) continue;
           e.visible = true;
        };
    };
    loopv(mms)
    {
        octaentities *oe = mms[i];
        loopv(oe->mapmodels)
        {
           extentity &e = *ents[oe->mapmodels[i]];
           if(!e.visible) continue;
           rendermapmodel(e);
           e.visible = false;
        };
    };
    if(reflected) restorevfcP();
};

void rendermapmodels()
{
    const vector<extentity *> &ents = et->getents();

    visiblemms = NULL;
    lastvisiblemms = &visiblemms;
    findvisiblemms(ents);

    static int skipoq = 0;

    renderedmms.setsizenodelete(0);

    for(octaentities *oe = visiblemms; oe; oe = oe->next)
    {
        bool occluded = oe->distance < 0;
        if(!occluded)
        {
            bool hasmodels = false;
            loopv(oe->mapmodels)
            {
                const extentity &e = *ents[oe->mapmodels[i]];
                if(!e.visible || (e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED)) continue;
                hasmodels = true;
                break;
            };
            if(!hasmodels) continue;
        };

        if(!hasOQ || !oqfrags || !oqmm || !oe->distance) oe->query = NULL;
        else if(!occluded && (++skipoq % oqmm)) oe->query = NULL;
        else oe->query = newquery(oe);

        if(oe->query)
        {
            if(occluded)
            {
                glDepthMask(GL_FALSE);
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            };
            startquery(oe->query);
        };
        if(!occluded || oe->query)
        {
            ivec bbmin(oe->o), bbmax(oe->o);
            bbmin.add(oe->size);
            bool rendered = false;
            loopv(oe->mapmodels)
            {
                extentity &e = *ents[oe->mapmodels[i]];
                if(e.attr3 && e.triggerstate == TRIGGER_DISAPPEARED) continue;
                if(occluded)
                {
                    ivec bo, br;
                    if(getentboundingbox(e, bo, br))
                    {
                        loopj(3)
                        {
                            bbmin[j] = min(bbmin[j], bo[j]);
                            bbmax[j] = max(bbmax[j], bo[j]+br[j]);
                        };
                    };
                }
                else if(e.visible)
                {
                    if(!rendered) { renderedmms.add(oe); rendered = true; };
                    rendermapmodel(e);
                    e.visible = false;
                };
            };
            if(occluded)
            {
                loopj(3)
                {
                    bbmin[j] = max(bbmin[j], oe->o[j]);
                    bbmax[j] = min(bbmax[j], oe->o[j]+oe->size);
                };
                drawbb(bbmin, bbmax.sub(bbmin));
            };
        };
        if(oe->query)
        {
            endquery(oe->query);
            if(occluded)
            {
                glDepthMask(GL_TRUE);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            };
        };
    };
};

bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size)
{
    loopoctabox(o, size, bo, br)
    {
        ivec co(i, o.x, o.y, o.z, size);
        if(c[i].ext && c[i].ext->va)
        {
            vtxarray *va = c[i].ext->va;
            if(va->curvfc >= VFC_FOGGED || va->occluded >= OCCLUDE_BB) continue;
        };
        if(c[i].children && bboccluded(bo, br, c[i].children, co, size>>1)) continue;
        return false;
    };
    return true;
};

VAR(outline, 0, 0, 0xFFFFFF);

void renderoutline()
{
    if(!editmode || !outline) return;

    notextureshader->set();

    glDisable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);

    glPushMatrix();

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3ub((outline>>16)&0xFF, (outline>>8)&0xFF, outline&0xFF);

    resetorigin();    
    GLuint vbufGL = 0, ebufGL = 0;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if(!lod.texs || va->occluded >= OCCLUDE_GEOM) continue;

        setorigin(va);

        bool vbufchanged = true;
        if(hasVBO)
        {
            if(vbufGL == va->vbufGL) vbufchanged = false;
            else
            {
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
                vbufGL = va->vbufGL;
            };
            if(ebufGL != lod.ebufGL)
            {
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, lod.ebufGL);
                ebufGL = lod.ebufGL;
            };
        };
        if(vbufchanged) glVertexPointer(3, floatvtx ? GL_FLOAT : GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), &(va->vbuf[0].x));

        glDrawElements(GL_TRIANGLES, 3*lod.tris, GL_UNSIGNED_SHORT, lod.ebuf);
        glde++;
        xtravertsva += va->verts;
    };

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    
    glPopMatrix();

    if(hasVBO)
    {
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    };
    glDisableClientState(GL_VERTEX_ARRAY);
    glEnable(GL_TEXTURE_2D);

    defaultshader->set();
};

float orientation_tangent [3][4] = { {  0,1, 0,0 }, { 1,0, 0,0 }, { 1,0,0,0 }};
float orientation_binormal[3][4] = { {  0,0,-1,0 }, { 0,0,-1,0 }, { 0,1,0,0 }};

struct renderstate
{
    bool colormask, depthmask, texture;
    GLuint vbufGL, ebufGL;
    float fogplane;
    Shader *shader;
    const ShaderParam *vertparams[MAXSHADERPARAMS], *pixparams[MAXSHADERPARAMS];

    renderstate() : colormask(true), depthmask(true), texture(true), vbufGL(0), ebufGL(0), fogplane(-1), shader(NULL)
    {
        memset(vertparams, 0, sizeof(vertparams));
        memset(pixparams, 0, sizeof(pixparams));
    };
};

#define setvertparam(param) \
    { \
        if(!cur.vertparams[param.index] || memcmp(cur.vertparams[param.index]->val, param.val, sizeof(param.val))) \
            glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 10+param.index, param.val); \
        cur.vertparams[param.index] = &param; \
    }

#define setpixparam(param) \
    { \
        if(!cur.pixparams[param.index] || memcmp(cur.pixparams[param.index]->val, param.val, sizeof(param.val))) \
            glProgramEnvParameter4fv_(GL_FRAGMENT_PROGRAM_ARB, 10+param.index, param.val); \
        cur.pixparams[param.index] = &param; \
    }

void renderquery(renderstate &cur, occludequery *query, vtxarray *va)
{
    setorigin(va);
    if(cur.shader!=nocolorshader) (cur.shader = nocolorshader)->set();
    if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); };
    if(cur.depthmask) { cur.depthmask = false; glDepthMask(GL_FALSE); };

    startquery(query);

    ivec origin(va->x, va->y, va->z);
    origin.mask(~VVEC_INT_MASK);

    vec camera(camera1->o);
    if(reflecting && !refracting) camera.z = reflecting;
    
    ivec bbmin, bbmax;
    if(va->children || va->mapmodels || va->l0.matsurfs || va->l0.sky || va->l0.explicitsky)
    {
        bbmin = ivec(va->x, va->y, va->z);
        bbmax = ivec(va->size, va->size, va->size);
    }
    else
    {
        bbmin = va->min;
        bbmax = va->max;
        bbmax.sub(bbmin);
    };

    drawbb(bbmin.sub(origin).mul(1<<VVEC_FRAC),
           bbmax.mul(1<<VVEC_FRAC),
           vec(camera).sub(origin.tovec()).mul(1<<VVEC_FRAC));

    endquery(query);
};

void renderva(renderstate &cur, vtxarray *va, lodlevel &lod, bool zfill = false)
{
    setorigin(va);
    bool vbufchanged = true;
    if(hasVBO)
    {
        if(cur.vbufGL == va->vbufGL) vbufchanged = false;
        else
        {
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
            cur.vbufGL = va->vbufGL;
        };
        if(cur.ebufGL != lod.ebufGL)
        {
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, lod.ebufGL);
            cur.ebufGL = lod.ebufGL;
        };
    };
    if(vbufchanged) glVertexPointer(3, floatvtx ? GL_FLOAT : GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), &(va->vbuf[0].x));
    if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); };

    if(zfill)
    {
        if(cur.shader != nocolorshader) (cur.shader = nocolorshader)->set();
        if(cur.colormask) { cur.colormask = false; glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); };
        glDrawElements(GL_TRIANGLES, 3*lod.tris, GL_UNSIGNED_SHORT, lod.ebuf);
        glde++;
        xtravertsva += va->verts;
        return;
    };

    if(refracting)
    {
        float fogplane = refracting - (va->z & ~VVEC_INT_MASK);
        if(cur.fogplane!=fogplane)
        {
            cur.fogplane = fogplane;
            setfogplane(0.5f, fogplane);
        };
    };
    if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); };

    extern int waterfog;
    if(refracting ? va->z+va->size<=refracting-waterfog : va->curvfc==VFC_FOGGED)
    {
        static Shader *fogshader = NULL;
        if(!fogshader) fogshader = lookupshaderbyname("fogworld");
        if(fogshader!=cur.shader) (cur.shader = fogshader)->set();
        if(cur.texture)
        {
            cur.texture = false;
            glDisable(GL_TEXTURE_2D);
            glActiveTexture_(GL_TEXTURE1_ARB);
            glDisable(GL_TEXTURE_2D);
            glActiveTexture_(GL_TEXTURE0_ARB);
        };
        glDrawElements(GL_TRIANGLES, 3*lod.tris, GL_UNSIGNED_SHORT, lod.ebuf);
        glde++;
        vtris += lod.tris;
        vverts += va->verts;
        return;
    };

    if(!cur.texture)
    {
        cur.texture = true;
        glEnable(GL_TEXTURE_2D);
        glActiveTexture_(GL_TEXTURE1_ARB);
        glEnable(GL_TEXTURE_2D);
        glActiveTexture_(GL_TEXTURE0_ARB);
    };

    if(renderpath!=R_FIXEDFUNCTION) 
    { 
        if(vbufchanged) glColorPointer(3, GL_UNSIGNED_BYTE, floatvtx ? sizeof(fvertex) : sizeof(vertex), floatvtx ? &(((fvertex *)va->vbuf)[0].n) : &(va->vbuf[0].n));
        glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 4, vec4(camera1->o, 1).sub(ivec(va->x, va->y, va->z).mask(~VVEC_INT_MASK).tovec()).mul(2).v);
    };

    if(vbufchanged)
    {
        glClientActiveTexture_(GL_TEXTURE1_ARB);
        glTexCoordPointer(2, GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), floatvtx ? &(((fvertex *)va->vbuf)[0].u) : &(va->vbuf[0].u));
        glClientActiveTexture_(GL_TEXTURE0_ARB);
    };

    ushort *ebuf = lod.ebuf;
    int lastlm = -1, lastxs = -1, lastys = -1, lastl = -1;
    Slot *lastslot = NULL;
    loopi(lod.texs)
    {
        Slot &slot = lookuptexture(lod.eslist[i].texture);
        Texture *tex = slot.sts[0].t;
        Shader *s = slot.shader;

        extern vector<GLuint> lmtexids;
        int lmid = lod.eslist[i].lmid, curlm = lmtexids[lmid];
        if(curlm!=lastlm || !lastslot || s->type!=lastslot->shader->type)
        {
            if(curlm!=lastlm)
            {
                glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, curlm);
                lastlm = curlm;
            };
            if(renderpath!=R_FIXEDFUNCTION && s->type==SHADER_NORMALSLMS && (lmid<LMID_RESERVED || lightmaps[lmid-LMID_RESERVED].type==LM_BUMPMAP0))
            {
                glActiveTexture_(GL_TEXTURE2_ARB);
                glBindTexture(GL_TEXTURE_2D, lmtexids[lmid+1]);
            };

            glActiveTexture_(GL_TEXTURE0_ARB);
        };

        if(&slot!=lastslot)
        {
            glBindTexture(GL_TEXTURE_2D, tex->gl);
            if(s!=cur.shader) (cur.shader = s)->set();

            if(renderpath!=R_FIXEDFUNCTION)
            {
                int tmu = s->type==SHADER_NORMALSLMS ? 3 : 2;
                loopvj(slot.sts)
                {
                    Slot::Tex &t = slot.sts[j];
                    if(t.type==TEX_DIFFUSE || t.combined>=0) continue;
                    glActiveTexture_(GL_TEXTURE0_ARB+tmu++);
                    glBindTexture(GL_TEXTURE_2D, t.t->gl);
                };
                uint vertparams = 0, pixparams = 0;
                loopvj(slot.params)
                {
                    const ShaderParam &param = slot.params[j];
                    if(param.type == SHPARAM_VERTEX)
                    {
                        setvertparam(param);
                        vertparams |= 1<<param.index;
                    }
                    else
                    {
                        setpixparam(param);
                        pixparams |= 1<<param.index;
                    };
                };
                loopvj(s->defaultparams)
                {
                    const ShaderParam &param = s->defaultparams[j];
                    if(param.type == SHPARAM_VERTEX)
                    {
                        if(!(vertparams & (1<<param.index))) setvertparam(param);
                    }
                    else if(!(pixparams & (1<<param.index))) setpixparam(param);
                };
                glActiveTexture_(GL_TEXTURE0_ARB);
            };
            lastslot = &slot;
        };

        loopl(3) if (lod.eslist[i].length[l])
        {
            if(lastl!=l || lastxs!=tex->xs || lastys!=tex->ys)
            {
                static int si[] = { 1, 0, 0 };
                static int ti[] = { 2, 2, 1 };

                GLfloat s[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                s[si[l]] = 8.0f/(tex->xs<<VVEC_FRAC);
                GLfloat t[] = { 0.0f, 0.0f, 0.0f, 0.0f };
                t[ti[l]] = (l <= 1 ? -8.0f : 8.0f)/(tex->ys<<VVEC_FRAC);

                if(renderpath==R_FIXEDFUNCTION)
                {
                    glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
                    glTexGenfv(GL_T, GL_OBJECT_PLANE, t);
                    // KLUGE: workaround for buggy nvidia drivers
                    // object planes are somehow invalid unless texgen is toggled
                    extern int nvidia_texgen_bug;
                    if(nvidia_texgen_bug)
                    {
                        glDisable(GL_TEXTURE_GEN_S);
                        glDisable(GL_TEXTURE_GEN_T);
                        glEnable(GL_TEXTURE_GEN_S);
                        glEnable(GL_TEXTURE_GEN_T);
                    };
                }
                else
                {
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 0, s);     // have to pass in env, otherwise same problem as fixed function
                    glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 1, t);
                };

                lastxs = tex->xs;
                lastys = tex->ys;
                lastl = l;
            };

            if(s->type>=SHADER_NORMALSLMS && renderpath!=R_FIXEDFUNCTION)
            {
                glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 2, orientation_tangent[l]);
                glProgramEnvParameter4fv_(GL_VERTEX_PROGRAM_ARB, 3, orientation_binormal[l]);
            };

            glDrawElements(GL_TRIANGLES, lod.eslist[i].length[l], GL_UNSIGNED_SHORT, ebuf);
            ebuf += lod.eslist[i].length[l];  // Advance to next array.
            glde++;
        };
    };

    vtris += lod.tris;
    vverts += va->verts;
};

VAR(oqdist, 0, 256, 1024);
VAR(zpass, 0, 1, 1);

extern int ati_texgen_bug;

void setupTMUs()
{
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glEnable(GL_TEXTURE_GEN_R);     // should not be needed, but apparently makes some ATI drivers happy

    if(renderpath!=R_FIXEDFUNCTION) glEnableClientState(GL_COLOR_ARRAY);

    setupTMU();

    glActiveTexture_(GL_TEXTURE1_ARB);
    glClientActiveTexture_(GL_TEXTURE1_ARB);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);

    glEnable(GL_TEXTURE_2D);
    setupTMU();
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef(1.0f/SHRT_MAX, 1.0f/SHRT_MAX, 1.0f);
    glMatrixMode(GL_MODELVIEW);

    glActiveTexture_(GL_TEXTURE0_ARB);
    glClientActiveTexture_(GL_TEXTURE0_ARB);

    if(renderpath!=R_FIXEDFUNCTION)
    {
        loopi(8-2) { glActiveTexture_(GL_TEXTURE2_ARB+i); glEnable(GL_TEXTURE_2D); };
        glActiveTexture_(GL_TEXTURE0_ARB);
        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 5, hdr.ambient/255.0f, hdr.ambient/255.0f, hdr.ambient/255.0f, 0);
    };

    glColor4f(1, 1, 1, 1);
};

void cleanupTMUs()
{
    if(hasVBO) 
    {
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    };
    glDisableClientState(GL_VERTEX_ARRAY);
    if(renderpath!=R_FIXEDFUNCTION)
    {
        glDisableClientState(GL_COLOR_ARRAY);
        loopi(8-2) { glActiveTexture_(GL_TEXTURE2_ARB+i); glDisable(GL_TEXTURE_2D); };
    };

    glActiveTexture_(GL_TEXTURE1_ARB);
    glClientActiveTexture_(GL_TEXTURE1_ARB);

    glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);

    glDisable(GL_TEXTURE_2D);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glActiveTexture_(GL_TEXTURE0_ARB);
    glClientActiveTexture_(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);

    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    if(ati_texgen_bug) glDisable(GL_TEXTURE_GEN_R);
};

#ifdef SHOWVA
VAR(showva, 0, 0, 1);
#endif

void rendergeom()
{
    glEnableClientState(GL_VERTEX_ARRAY);

    if(!zpass) setupTMUs();

    flipqueries();

    vtris = vverts = 0;

    glPushMatrix();

    resetorigin();

    renderstate cur;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if(!lod.texs) continue;
        if(hasOQ && oqfrags && (zpass || va->distance > oqdist) && !insideva(va, camera1->o))
        {
            if(!zpass && va->query && va->query->owner == va) 
                va->occluded = checkquery(va->query) ? min(va->occluded+1, OCCLUDE_BB) : OCCLUDE_NOTHING;
            if(zpass && va->parent && 
               (va->parent->occluded == OCCLUDE_PARENT || 
                (va->parent->occluded >= OCCLUDE_BB && 
                 va->parent->query && va->parent->query->owner == va->parent && va->parent->query->fragments < 0)))
            {
                va->query = NULL;
                if(va->occluded >= OCCLUDE_GEOM)
                {
                    va->occluded = OCCLUDE_PARENT;
                    continue;
                };
            }
            else if(va->occluded >= OCCLUDE_GEOM)
            {
                va->query = newquery(va);
                if(va->query) renderquery(cur, va->query, va);
                continue;
            }
            else va->query = newquery(va);
        }
        else
        {
            va->query = NULL;
            va->occluded = OCCLUDE_NOTHING;
        };


        if(va->query) startquery(va->query);

        renderva(cur, va, lod, zpass!=0);

        if(va->query) endquery(va->query);
    };

    if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); };
    if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); };

    if(zpass) 
    {
        setupTMUs();
        glDepthFunc(GL_LEQUAL);
#ifdef SHOWVA
        int showvas = 0;
#endif
        cur.vbufGL = 0;
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            lodlevel &lod = va->curlod ? va->l1 : va->l0;
            if(!lod.texs) continue;
            if(va->parent && va->parent->occluded >= OCCLUDE_BB && (!va->parent->query || va->parent->query->fragments >= 0)) 
            {
                va->query = NULL;
                va->occluded = OCCLUDE_BB;
                continue;
            }
            else if(va->query)
            {
                va->occluded = checkquery(va->query) ? min(va->occluded+1, OCCLUDE_BB) : OCCLUDE_NOTHING;
                if(va->occluded >= OCCLUDE_GEOM) continue;
            }
            else if(va->occluded == OCCLUDE_PARENT) va->occluded = OCCLUDE_NOTHING;

#ifdef SHOWVA
            if(showva && editmode && renderpath==R_FIXEDFUNCTION)
            {
                if(insideva(va, worldpos)) 
                {
                    glColor3f(1, showvas/3.0f, 1-showvas/3.0f);
                    showvas++;
                }
                else glColor3f(1, 1, 1);
            };
#endif
            renderva(cur, va, lod);
        };
        glDepthFunc(GL_LESS);
    };

    glPopMatrix();
    cleanupTMUs();
};

void findreflectedvas(vector<vtxarray *> &vas, float z, bool refract, bool vfc = true)
{
    bool doOQ = hasOQ && oqfrags && oqreflect;
    loopv(vas)
    {
        vtxarray *va = vas[i];
        if(!vfc) va->curvfc = VFC_NOT_VISIBLE;
        if(va->curvfc == VFC_FOGGED || va->z+va->size <= z || isvisiblecube(vec(va->x, va->y, va->z), va->size) >= VFC_FOGGED) continue;
        bool render = true;
        if(va->curvfc == VFC_FULL_VISIBLE)
        {
            if(va->occluded >= OCCLUDE_BB) continue;
            if(va->occluded >= OCCLUDE_GEOM) render = false;
        };
        if(render)
        {
            if(va->curvfc == VFC_NOT_VISIBLE) va->distance = (int)vadist(va, camera1->o);
            if(!doOQ && va->distance > reflectdist) continue;
            va->rquery = NULL;
            vtxarray **vprev = &reflectedva, *vcur = reflectedva;
            while(vcur && va->distance > vcur->distance)
            {
                vprev = &vcur->rnext;
                vcur = vcur->rnext;
            };
            va->rnext = *vprev;
            *vprev = va;
        };
        if(va->children->length()) findreflectedvas(*va->children, z, refract, va->curvfc != VFC_NOT_VISIBLE);
    };
};

void renderreflectedgeom(float z, bool refract)
{
    glEnableClientState(GL_VERTEX_ARRAY);
    setupTMUs();
    glPushMatrix();

    resetorigin();

    renderstate cur;
    if(!refract && camera1->o.z >= z)
    {
        reflectvfcP(z);
        reflectedva = NULL;
        findreflectedvas(varoot, z, refract);
        bool doOQ = hasOQ && oqfrags && oqreflect;
        for(vtxarray *va = reflectedva; va; va = va->rnext)
        {
            lodlevel &lod = va->curlod ? va->l1 : va->l0;
            if(!lod.texs || va->max.z <= z) continue;
            if(doOQ)
            {
                va->rquery = newquery(&va->rquery);
                if(!va->rquery) continue;
                if(va->occluded >= OCCLUDE_BB || va->curvfc == VFC_NOT_VISIBLE)
                {
                    renderquery(cur, va->rquery, va);
                    continue;
                };
            };
            if(va->rquery) startquery(va->rquery);
            renderva(cur, va, lod, doOQ);
            if(va->rquery) endquery(va->rquery);
        };            
        if(doOQ)
        {
            glDepthFunc(GL_LEQUAL);
            if(!cur.colormask) { cur.colormask = true; glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); };
            if(!cur.depthmask) { cur.depthmask = true; glDepthMask(GL_TRUE); };
            cur.vbufGL = 0;
            for(vtxarray **prevva = &reflectedva, *va = reflectedva; va; prevva = &va->rnext, va = va->rnext)
            {
                lodlevel &lod = va->curlod ? va->l1 : va->l0;
                if(!lod.texs || va->max.z <= z) continue;
                if(va->rquery && checkquery(va->rquery)) 
                {
                    if(va->occluded >= OCCLUDE_BB || va->curvfc == VFC_NOT_VISIBLE) *prevva = va->rnext;
                    continue;
                };
                renderva(cur, va, lod);
            };
            glDepthFunc(GL_LESS);
        };
        restorevfcP();
    }
    else
    {
        for(vtxarray *va = visibleva; va; va = va->next)
        {
            lodlevel &lod = va->curlod ? va->l1 : va->l0;
            if(!lod.texs) continue;
            if(va->curvfc == VFC_FOGGED || (refract && camera1->o.z >= z ? va->min.z > z : va->max.z <= z) || va->occluded >= OCCLUDE_GEOM) continue;
            if((!hasOQ || !oqfrags) && va->distance > reflectdist) break;
            renderva(cur, va, lod);
        };
    };

    glPopMatrix();
    cleanupTMUs();
};

static GLuint skyvbufGL, skyebufGL;

void renderskyva(vtxarray *va, lodlevel &lod, bool explicitonly = false)
{
    setorigin(va);

    bool vbufchanged = true;
    if(hasVBO)
    {
        if(skyvbufGL == va->vbufGL) vbufchanged = false;
        else
        {
            glBindBuffer_(GL_ARRAY_BUFFER_ARB, va->vbufGL);
            glVertexPointer(3, floatvtx ? GL_FLOAT : GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), &(va->vbuf[0].x));
            skyvbufGL = va->vbufGL;
        };
        if(skyebufGL != lod.skybufGL)
        {
            glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, lod.skybufGL);
            skyebufGL = lod.skybufGL;
        };
    };
    if(vbufchanged) glVertexPointer(3, floatvtx ? GL_FLOAT : GL_SHORT, floatvtx ? sizeof(fvertex) : sizeof(vertex), &(va->vbuf[0].x));

    glDrawElements(GL_TRIANGLES, explicitonly  ? lod.explicitsky : lod.sky+lod.explicitsky, GL_UNSIGNED_SHORT, explicitonly ? lod.skybuf+lod.sky : lod.skybuf);
    glde++;

    if(!explicitonly) xtraverts += lod.sky/3;
    xtraverts += lod.explicitsky/3;
};

void renderreflectedskyvas(vector<vtxarray *> &vas, float z, bool vfc = true)
{
    loopv(vas)
    {
        vtxarray *va = vas[i];
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if((vfc && va->curvfc == VFC_FULL_VISIBLE) && va->occluded >= OCCLUDE_BB) continue;
        if(va->z+va->size <= z || isvisiblecube(vec(va->x, va->y, va->z), va->size) == VFC_NOT_VISIBLE) continue;
        if(lod.sky+lod.explicitsky) renderskyva(va, lod);
        if(va->children->length()) renderreflectedskyvas(*va->children, z, vfc && va->curvfc != VFC_NOT_VISIBLE);
    };
};

void rendersky(bool explicitonly, float zreflect)
{
    glEnableClientState(GL_VERTEX_ARRAY);

    glPushMatrix();

    resetorigin();

    skyvbufGL = skyebufGL = 0;
 
    if(zreflect)
    {
        reflectvfcP(zreflect);
        renderreflectedskyvas(varoot, zreflect);
        restorevfcP();
    }
    else for(vtxarray *va = visibleva; va; va = va->next)
    {
        lodlevel &lod = va->curlod ? va->l1 : va->l0;
        if(va->occluded >= OCCLUDE_BB || !(explicitonly ? lod.explicitsky : lod.sky+lod.explicitsky)) continue;

        renderskyva(va, lod, explicitonly);
    };

    glPopMatrix();

    if(hasVBO)
    {
        glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
        glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    };
    glDisableClientState(GL_VERTEX_ARRAY);
};

void writeobj(char *name)
{
    bool oldVBO = hasVBO;
    hasVBO = false;
    allchanged();
    s_sprintfd(fname)("%s.obj", name);
    FILE *f = fopen(fname, "w");
    if(!f) return;
    fprintf(f, "# obj file of sauerbraten level\n");
    loopv(valist)
    {
        vtxarray &v = *valist[i];
        vertex *verts = v.vbuf;
        if(verts)
        {
            loopj(v.verts) fprintf(f, "v %d %d %d\n", verts[j].x, verts[j].y, verts[j].z);
            lodlevel &lod = v.curlod ? v.l1 : v.l0;
            ushort *ebuf = lod.ebuf;
            loopi(lod.texs) loopl(3) loopj(lod.eslist[i].length[l]/3)
            {
                fprintf(f, "f");
                for(int k = 3; k>=0; k--) fprintf(f, " %d", ebuf[k]-v.verts);
                ebuf += 3;
                fprintf(f, "\n");
            };
        };
    };
    fclose(f);
    hasVBO = oldVBO;
    allchanged();
};

COMMAND(writeobj, "s");
