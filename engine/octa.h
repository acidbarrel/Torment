// 6-directional octree heightfield map format

struct elementset
{
    int texture;
    int lmid;
    int length[3];
};

struct materialsurface
{
    ivec o;
    ushort csize, rsize;
    union
    {
        short index;
        short depth;
    };
    uchar material, orient;
    union
    {
        entity *light;
        GLuint tex;
    };
};

struct lodlevel
{
    elementset *eslist;      // List of element indces sets (range) per texture
    ushort *ebuf;            // packed element indices buffer
    ushort *skybuf;          // skybox packed element indices buffer
    materialsurface *matbuf; // buffer of material surfaces
    GLuint ebufGL, skybufGL;  // element index VBO
    int tris, texs, matsurfs, sky, explicitsky;
};

struct occludequery
{
    void *owner;
    GLuint id;
    int fragments;
};

struct vtxarray;

struct octaentities
{
    vector<int> mapmodels;
    vector<int> other;
    occludequery *query;
    octaentities *next;
    int distance;
    ivec o;
    int size;

    octaentities(const ivec &o, int size) : query(0), o(o), size(size) {};
};

enum
{
    OCCLUDE_NOTHING = 0,
    OCCLUDE_GEOM,
    OCCLUDE_BB,
    OCCLUDE_PARENT
};

enum
{
    MERGE_ORIGIN = 1<<0,
    MERGE_PART   = 1<<1,
    MERGE_USE    = 1<<2
};

struct vtxarray
{
    vtxarray *parent;
    vector<vtxarray *> *children;
    lodlevel l0, l1;
    vertex *vbuf;           // vertex buffer
    vtxarray *next, *rnext; // linked list of visible VOBs
    int allocsize;          // size of allocated memory for this va
    int verts, explicitsky, skyarea, curlod, distance;
    GLuint vbufGL;            // VBO buffer ID
    int x, y, z, size;      // location and size of cube.
    ivec min, max;          // BB
    uchar curvfc, occluded;
    occludequery *query, *rquery;
    vector<octaentities *> *mapmodels;
    int hasmerges;
};

struct surfaceinfo
{
    uchar texcoords[8];
    uchar w, h;
    ushort x, y, lmid;
};

struct surfacenormals
{
    bvec normals[4];
};

struct clipplanes
{
    vec o, r;
    int size;
    plane p[12];
    clipplanes *next, *prev;
    clipplanes **backptr;
};

struct mergeinfo
{
    short u1, u2, v1, v2;
};

struct cubeext
{
    uchar material;          // empty-space material
    uchar visible;           // visible faces of the cube
    uchar merged;            // merged faces of the cube
    uchar mergeorigin;       // whether this face describes a larger merged face
    vtxarray *va;            // vertex array for children, or NULL
    clipplanes *clip;        // collision planes
    surfaceinfo *surfaces;   // lighting info for each surface
    surfacenormals *normals; // per-vertex normals for each surface
    octaentities *ents;      // list of map entites totally inside cube
    mergeinfo *merges;       // bounds of merged surfaces
};  

struct cube
{
    cube *children;          // points to 8 cube structures which are its children, or NULL. -Z first, then -Y, -X
    union
    {
        uchar edges[12];     // edges of the cube, each uchar is 2 4bit values denoting the range.
                             // see documentation jpgs for more info.
        uint faces[3];       // 4 edges of each dimension together representing 2 perpendicular faces
    };
    ushort texture[6];       // one for each face. same order as orient.
    cubeext *ext;            // extended info for the cube
};

static inline cubeext &ext(cube &c)
{
    extern cubeext *newcubeext(cube &c);
    return *(c.ext ? c.ext : newcubeext(c));
};

struct block3
{
    ivec o, s;
    int grid, orient;
    block3() {};
    block3(const selinfo &sel) : o(sel.o), s(sel.s), grid(sel.grid), orient(sel.orient) {};
    cube *c()           { return (cube *)(this+1); };
    int size()    const { return s.x*s.y*s.z; };
};

struct editinfo
{
    block3 *copy;
    editinfo() : copy(NULL) {};
};

struct undoent   { int i; entity e; };
struct undoblock { int *g, n; block3 *b; undoent *e; undoblock() : g(NULL), n(0), b(NULL), e(NULL) {}; };

extern cube *worldroot;             // the world data. only a ptr to 8 cubes (ie: like cube.children above)
extern ivec lu;
extern int lusize;
extern bool luperfect;
extern int wtris, wverts, vtris, vverts, glde, rplanes;
extern int allocnodes, allocva, selchildcount;

const uint F_EMPTY = 0;             // all edges in the range (0,0)
const uint F_SOLID = 0x80808080;    // all edges in the range (0,8)

#define isempty(c) ((c).faces[0]==F_EMPTY)
#define isentirelysolid(c) ((c).faces[0]==F_SOLID && (c).faces[1]==F_SOLID && (c).faces[2]==F_SOLID)
#define setfaces(c, face) { (c).faces[0] = (c).faces[1] = (c).faces[2] = face; }
#define solidfaces(c) setfaces(c, F_SOLID)
#define emptyfaces(c) setfaces(c, F_EMPTY)

#define edgemake(a, b) ((b)<<4|a)
#define edgeget(edge, coord) ((coord) ? (edge)>>4 : (edge)&0xF)
#define edgeset(edge, coord, val) ((edge) = ((coord) ? ((edge)&0xF)|((val)<<4) : ((edge)&0xF0)|(val)))

#define cubeedge(c, d, x, y) ((c).edges[(((d)<<2)+((y)<<1)+(x))])

#define octadim(d)          (1<<(d))                    // creates mask for bit of given dimension
#define octacoord(d, i)     (((i)&octadim(d))>>(d))
#define oppositeocta(d, i)  ((i)^octadim(D[d]))
#define octaindex(d,x,y,z)  (octadim(D[d])*(z)+octadim(C[d])*(y)+octadim(R[d])*(x))

#define loopoctabox(c, size, o, s) uchar possible = octantrectangleoverlap(c, size, o, s); loopi(8) if(possible&(1<<i))

enum
{
    O_LEFT = 0,
    O_RIGHT,
    O_BACK,
    O_FRONT,
    O_BOTTOM,
    O_TOP
};

#define dimension(orient) ((orient)>>1)
#define dimcoord(orient)  ((orient)&1)
#define opposite(orient)  ((orient)^1)

enum
{
    VFC_FULL_VISIBLE = 0,
    VFC_PART_VISIBLE,
    VFC_FOGGED,
    VFC_NOT_VISIBLE
};

struct block { int x, y, xs, ys; };
