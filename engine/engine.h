#include "cube.h"
#include "iengine.h"
#include "igame.h"

#include "world.h"
#include "octa.h"
#include "lightmap.h"
#include "shaders.h"
#include "spheretree.h"

struct Texture
{
    char *name;
    int xs, ys, w, h, bpp;
    GLuint gl;
};

struct SphereTree;

enum { MDL_MD2 = 1, MDL_MD3 };

struct model
{
    Shader *shader;
    float spec, ambient;
    bool collide, cullface, masked, shadow;
    float scale;
    vec translate;
    SphereTree *spheretree;
    vec bbcenter, bbradius;
    float eyeheight, collideradius, collideheight;

    model() : shader(0), spec(1.0f), ambient(0.3f), collide(true), cullface(true), masked(false), shadow(true), scale(1.0f), translate(0, 0, 0), spheretree(0), bbcenter(0, 0, 0), bbradius(0, 0, 0), eyeheight(0.9f), collideradius(0), collideheight(0) {};
    virtual ~model() { DELETEP(spheretree); };
    virtual void calcbb(int frame, vec &center, vec &radius) = 0;
    virtual void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl = NULL) = 0;
    virtual void setskin(int tex = 0) = 0;
    virtual bool load() = 0;
    virtual char *name() = 0;
    virtual int type() = 0;
    virtual SphereTree *setspheretree() { return 0; };

    void boundbox(int frame, vec &center, vec &radius)
    {
        if(frame) calcbb(frame, center, radius);
        else
        {
            if(bbradius.iszero()) calcbb(0, bbcenter, bbradius);
            center = bbcenter;
            radius = bbradius;
        };
    };

    void collisionbox(int frame, vec &center, vec &radius)
    {
        boundbox(frame, center, radius);
        if(collideradius) 
        {
            center[0] = center[1] = 0;
            radius[0] = radius[1] = collideradius;
        };
        if(collideheight)
        {
            center[2] = radius[2] = collideheight/2;
        };
    };

    float boundsphere(int frame, vec &center)
    {
        vec radius;
        boundbox(frame, center, radius);
        return radius.magnitude();
    };

    float above(int frame = 0)
    {
        vec center, radius;
        boundbox(frame, center, radius);
        return center.z+radius.z;
    };

    void setshader()
    {
        if(renderpath==R_FIXEDFUNCTION) return;

        if(shader) shader->set();
        else
        {
            static Shader *modelshader = NULL, *modelshadernospec = NULL, *modelshadermasks = NULL;            

            if(!modelshader)       modelshader       = lookupshaderbyname("stdppmodel");
            if(!modelshadernospec) modelshadernospec = lookupshaderbyname("nospecpvmodel");
            if(!modelshadermasks)  modelshadermasks  = lookupshaderbyname("masksppmodel");

            (masked ? modelshadermasks : (spec>=0.01f ? modelshader : modelshadernospec))->set();
        };

        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 2, spec, spec, spec, 0);

        GLfloat color[4];
        glGetFloatv(GL_CURRENT_COLOR, color);
        vec diffuse = vec(color).mul(ambient);
        loopi(3) diffuse[i] = max(diffuse[i], 0.2f);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 3, diffuse.x, diffuse.y, diffuse.z, 1);
        glProgramEnvParameter4f_(GL_FRAGMENT_PROGRAM_ARB, 3, diffuse.x, diffuse.y, diffuse.z, 1);
    };
};

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

enum
{
    TEX_DIFFUSE = 0,
    TEX_UNKNOWN,
    TEX_DECAL,
    TEX_NORMAL,
    TEX_GLOW,
    TEX_SPEC,
    TEX_DEPTH,
};

struct Slot
{
    struct Tex
    {
        int type;
        Texture *t;
        string name;
        int rotation, xoffset, yoffset;
        int combined;
    };

    vector<Tex> sts;
    Shader *shader;
    vector<ShaderParam> params;
    bool loaded;

    void reset()
    {
        sts.setsize(0);
        shader = NULL;
        params.setsize(0);
        loaded = false;
    };
};


// GL_ARB_multitexture
extern PFNGLACTIVETEXTUREARBPROC       glActiveTexture_;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_;
extern PFNGLMULTITEXCOORD2FARBPROC     glMultiTexCoord2f_;

// GL_ARB_vertex_buffer_object
extern PFNGLGENBUFFERSARBPROC    glGenBuffers_;
extern PFNGLBINDBUFFERARBPROC    glBindBuffer_;
extern PFNGLMAPBUFFERARBPROC     glMapBuffer_;
extern PFNGLUNMAPBUFFERARBPROC   glUnmapBuffer_;
extern PFNGLBUFFERDATAARBPROC    glBufferData_;
extern PFNGLBUFFERSUBDATAARBPROC glBufferSubData_;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffers_;

// GL_ARB_occlusion_query
extern PFNGLGENQUERIESARBPROC        glGenQueries_;
extern PFNGLDELETEQUERIESARBPROC     glDeleteQueries_;
extern PFNGLBEGINQUERYARBPROC        glBeginQuery_;
extern PFNGLENDQUERYARBPROC          glEndQuery_;
extern PFNGLGETQUERYIVARBPROC        glGetQueryiv_;
extern PFNGLGETQUERYOBJECTIVARBPROC  glGetQueryObjectiv_;
extern PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuiv_;

// GL_EXT_framebuffer_object
extern PFNGLBINDRENDERBUFFEREXTPROC        glBindRenderbuffer_;
extern PFNGLDELETERENDERBUFFERSEXTPROC     glDeleteRenderbuffers_;
extern PFNGLGENFRAMEBUFFERSEXTPROC         glGenRenderbuffers_;
extern PFNGLRENDERBUFFERSTORAGEEXTPROC     glRenderbufferStorage_;
extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC  glCheckFramebufferStatus_;
extern PFNGLBINDFRAMEBUFFEREXTPROC         glBindFramebuffer_;
extern PFNGLDELETEFRAMEBUFFERSEXTPROC      glDeleteFramebuffers_;
extern PFNGLGENFRAMEBUFFERSEXTPROC         glGenFramebuffers_;
extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC    glFramebufferTexture2D_;
extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbuffer_;

#define FONTH 64
#define MINRESW 640
#define MINRESH 480

extern dynent *player;
extern physent *camera1;                // special ent that acts as camera, same object as player1 in FPS mode

extern header hdr;                      // current map header
extern vector<ushort> texmru;
extern int xtraverts, xtravertsva;
extern vector<vertex> verts;            // the vertex array for all world rendering
extern int curtexnum;
extern const ivec cubecoords[8];
extern const ushort fv[6][4];
extern const uchar faceedgesidx[6][4];
extern Texture *crosshair;
extern bool inbetweenframes;

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time

extern igameclient     *cl;
extern igameserver     *sv;
extern iclientcom      *cc;
extern icliententities *et;

extern vector<int> entgroup;

// rendergl
extern bool hasVBO, hasOQ, hasFBO, hasCM, hasTC;

struct cubemapside
{
    GLenum target;
    const char *name;
};

extern cubemapside cubemapsides[6];

extern void gl_init(int w, int h, int bpp, int depth, int fsaa);
extern void cleangl();
extern void gl_drawframe(int w, int h, float curfps);
extern Texture *textureload(const char *name, bool clamp = false, bool mipit = true, bool msg = true);
extern GLuint cubemapfromsky(int size);
extern Texture *cubemapload(const char *name, bool mipit = true, bool msg = true);
extern void drawcubemap(int size, const vec &o, float yaw, float pitch);
extern Slot    &lookuptexture(int tex, bool load = true);
extern Shader  *lookupshader(int slot);
extern void createtexture(int tnum, int w, int h, void *pixels, bool clamp, bool mipit, GLenum component = GL_RGB, GLenum target = GL_TEXTURE_2D);
extern void setfogplane(float scale = 0, float z = 0);

// renderextras
extern void render3dbox(vec &o, float tofloor, float toceil, float xradius, float yradius = 0);


// octa
extern cube *newcubes(uint face = F_EMPTY);
extern cubeext *newcubeext(cube &c);
extern void getcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern void setcubevector(cube &c, int d, int x, int y, int z, ivec &p);
extern int familysize(cube &c);
extern void freeocta(cube *c);
extern void discardchildren(cube &c);
extern void optiface(uchar *p, cube &c);
extern void validatec(cube *c, int size);
extern bool isvalidcube(cube &c);
extern cube &lookupcube(int tx, int ty, int tz, int tsize = 0);
extern cube &neighbourcube(int x, int y, int z, int size, int rsize, int orient);
extern void newclipplanes(cube &c);
extern void freeclipplanes(cube &c);
extern uchar octantrectangleoverlap(const ivec &c, int size, const ivec &o, const ivec &s);
extern void forcemip(cube &c);
extern bool subdividecube(cube &c, bool fullcheck=true, bool brighten=true);
extern int faceverts(cube &c, int orient, int vert);
extern void calcvert(cube &c, int x, int y, int z, int size, vvec &vert, int i, bool solid = false);
extern void calcverts(cube &c, int x, int y, int z, int size, vvec *verts, bool *usefaces, int *vertused, bool lodcube);
extern uint faceedges(cube &c, int orient);
extern bool collapsedface(uint cfe);
extern bool touchingface(cube &c, int orient);
extern int genclipplane(cube &c, int i, vec *v, plane *clip);
extern void genclipplanes(cube &c, int x, int y, int z, int size, clipplanes &p);
extern bool visibleface(cube &c, int orient, int x, int y, int z, int size, uchar mat = MAT_AIR, bool lodcube = false);
extern int visibleorient(cube &c, int orient);
extern bool threeplaneintersect(plane &pl1, plane &pl2, plane &pl3, vec &dest);
extern void remipworld();
extern void freemergeinfo(cube &c);
extern void genmergedverts(cube &cu, int orient, const ivec &co, int size, const mergeinfo &m, vvec *vv, plane *p = NULL);
extern int calcmergedsize(int orient, const ivec &co, int size, const mergeinfo &m, const vvec *vv);
extern void invalidatemerges(cube &c);
extern void calcmerges();

struct cubeface : mergeinfo
{
    cube *c;
};

extern int mergefaces(int orient, cubeface *m, int sz);
extern void mincubeface(cube &cu, int orient, const ivec &o, int size, const mergeinfo &orig, mergeinfo &cf);

// ents
extern bool haveselent();
extern int rayent(const vec &o, vec &ray);
extern void copyundoents(undoblock &d, undoblock &s);
extern void pasteundoents(undoblock &u);

// octaedit
extern void cancelsel();
extern void render_texture_panel(int w, int h);
extern void addundo(undoblock &u);

// octarender
extern void visiblecubes(cube *c, int size, int cx, int cy, int cz, int w, int h, int fov);
extern void reflectvfcP(float z);
extern void restorevfcP();
extern void octarender();
extern void rendermapmodels();
extern void rendergeom();
extern void renderoutline();
extern void allchanged(bool load = false);
extern void rendersky(bool explicitonly = false, float zreflect = 0);
extern void converttovectorworld();

extern void vaclearc(cube *c);
extern vtxarray *newva(int x, int y, int z, int size);
extern void destroyva(vtxarray *va, bool reparent = true);
extern int isvisiblesphere(float rad, const vec &cv);
extern bool bboccluded(const ivec &bo, const ivec &br, cube *c, const ivec &o, int size);
extern occludequery *newquery(void *owner);
extern bool checkquery(occludequery *query, bool nowait = false);
extern void resetqueries();
extern int getnumqueries();

#define startquery(query) { glBeginQuery_(GL_SAMPLES_PASSED_ARB, (query)->id); }
#define endquery(query) \
    { \
        glEndQuery_(GL_SAMPLES_PASSED_ARB); \
        extern int ati_oq_bug; \
        if(ati_oq_bug) glFlush(); \
    }

// water

#define getwatercolour(wcol) \
    uchar wcol[3] = { 20, 70, 80 }; \
    if(hdr.watercolour[0] || hdr.watercolour[1] || hdr.watercolour[2]) memcpy(wcol, hdr.watercolour, 3);

extern int showmat;

extern int findmaterial(const char *name);
extern void genmatsurfs(cube &c, int cx, int cy, int cz, int size, vector<materialsurface> &matsurfs);
extern void rendermatsurfs(materialsurface *matbuf, int matsurfs);
extern void rendermatgrid(materialsurface *matbuf, int matsurfs);
extern int optimizematsurfs(materialsurface *matbuf, int matsurfs);
extern void setupmaterials(bool load = false);
extern void cleanreflections();
extern void queryreflections();
extern void drawreflections();
extern void renderwater();
extern void rendermaterials(float zclip = 0, bool refract = false);

// server

extern void initserver(bool dedicated);
extern void cleanupserver();
extern void serverslice(int seconds, uint timeout);

extern uchar *retrieveservers(uchar *buf, int buflen);
extern void localclienttoserver(int chan, ENetPacket *);
extern void localconnect();
extern bool serveroption(char *opt);

// serverbrowser
extern bool resolverwait(const char *name, ENetAddress *address);
extern void addserver(char *servername);
extern char *getservername(int n);
extern void writeservercfg();

// client
extern void localdisconnect();
extern void localservertoclient(int chan, uchar *buf, int len);
extern void connects(char *servername);
extern void clientkeepalive();

// command
extern bool overrideidents, persistidents;

extern void clearoverrides();
extern void writecfg();

// console
extern void writebinds(FILE *f);

// main
extern void estartmap(const char *name);
extern void computescreen(const char *text);

// menu
extern void menuprocess();

// physics
extern void mousemove(int dx, int dy);
extern bool pointincube(const clipplanes &p, const vec &v);
extern bool overlapsdynent(const vec &o, float radius);
extern void rotatebb(vec &center, vec &radius, int yaw);

// world
enum
{
    TRIG_COLLIDE    = 1<<0,
    TRIG_TOGGLE     = 1<<1,
    TRIG_ONCE       = 0<<2,
    TRIG_MANY       = 1<<2,
    TRIG_DISAPPEAR  = 1<<3,
    TRIG_AUTO_RESET = 1<<4,
    TRIG_RUMBLE     = 1<<5,
    TRIG_LOCKED     = 1<<6,
};

#define NUMTRIGGERTYPES 16

extern int triggertypes[NUMTRIGGERTYPES];

#define checktriggertype(type, flag) (triggertypes[(type) & (NUMTRIGGERTYPES-1)] & (flag))

extern void entitiesinoctanodes();
extern void freeoctaentities(cube &c);
extern bool pointinsel(selinfo &sel, vec &o);

// lightmap
extern void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2 = 0, const char *text2 = NULL);

// rendermodel
struct mapmodelinfo { string name; int tex; model *m; };

extern int findanim(const char *name);
extern void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks, model *m);
extern model *loadmodel(const char *name, int i = -1);
extern mapmodelinfo &getmminfo(int i);

// particles
extern void particleinit();

// 3dgui
extern void g3d_render();
extern bool g3d_windowhit(bool on, bool act);

extern void g3d_mainmenu();
