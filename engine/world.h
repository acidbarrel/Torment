
enum                            // hardcoded texture numbers
{
    DEFAULT_SKY = 0,
    DEFAULT_LIQUID,
    DEFAULT_WALL,
    DEFAULT_FLOOR,
    DEFAULT_CEIL
};

#define MAPVERSION 21           // bump if map format changes, see worldio.cpp

struct header                   // map file format header
{
    char head[4];               // "OCTA"
    int version;                // any >8bit quantity is a little indian
    int headersize;             // sizeof(header)
    int worldsize;
    int numents;
    int waterlevel;
    int lightmaps;
    int mapprec, maple, mapllod;
    uchar ambient;
    uchar watercolour[3];
    uchar mapwlod;
    uchar lerpangle, lerpsubdiv, lerpsubdivsize;
    uchar mapbe;
    uchar reserved1[3];
    int reserved2[4];
    char maptitle[128];
};

enum                            // cube empty-space materials
{
    MAT_AIR = 0,                // the default, fill the empty space with air
    MAT_WATER,                  // fill with water, showing waves at the surface
    MAT_CLIP,                   // collisions always treat cube as solid
    MAT_GLASS,                  // behaves like clip but is blended blueish
    MAT_NOCLIP,                 // collisions always treat cube as empty
    MAT_EDIT                    // basis for the edit volumes of the above materials
};

enum 
{ 
    MATSURF_NOT_VISIBLE = 0,
    MATSURF_VISIBLE,
    MATSURF_EDIT_ONLY
};

#define isclipped(mat) ((mat) >= MAT_CLIP && (mat) < MAT_NOCLIP)

#define VVEC_INT  14
#define VVEC_FRAC 1
#define VVEC_BITS (VVEC_INT + VVEC_FRAC)

#define VVEC_INT_MASK     ((1<<(VVEC_INT-1))-1)
#define VVEC_INT_COORD(n) (((n)&VVEC_INT_MASK)<<VVEC_FRAC)

struct vvec : svec
{
    vvec() {};
    vvec(short x, short y, short z) : svec(x, y, z) {};
    vvec(int x, int y, int z) : svec(VVEC_INT_COORD(x), VVEC_INT_COORD(y), VVEC_INT_COORD(z)) {};
    vvec(const int *i) : svec(VVEC_INT_COORD(i[0]), VVEC_INT_COORD(i[1]), VVEC_INT_COORD(i[2])) {};

    void mask(int f) { f <<= VVEC_FRAC; f |= (1<<VVEC_FRAC)-1; x &= f; y &= f; z &= f; };

    vec tovec() const                    { return vec(x, y, z).div(1<<VVEC_FRAC); };
    vec tovec(int x, int y, int z) const { vec t = tovec(); t.x += x&~VVEC_INT_MASK; t.y += y&~VVEC_INT_MASK; t.z += z&~VVEC_INT_MASK; return t; };
    vec tovec(const ivec &o) const       { return tovec(o.x, o.y, o.z); };
};

struct vertex : vvec { short u, v; bvec n; };
struct fvertex : vec { short u, v; bvec n; };
