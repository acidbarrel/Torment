// renderextras.cpp: misc gl render code and the HUD

#include "pch.h"
#include "engine.h"


void writetgaheader(FILE *f, SDL_Surface *s, int bits)
{
    fwrite("\0\0\x02\0\0\0\0 \0\0\0\0", 1, 12, f);
    ushort dim[] = { s->w, s->h };
    endianswap(dim, sizeof(ushort), 2);
    fwrite(dim, sizeof(short), 2, f);
    fputc(bits, f);
    fputc(0, f);
}

void flipnormalmapy(char *destfile, char *normalfile)           // RGB (jpg/png) -> BGR (tga)
{
    SDL_Surface *ns = IMG_Load(normalfile);
    if(!ns) return;
    FILE *f = fopen(destfile, "wb");
    if(f)
    {
        writetgaheader(f, ns, 24);
        for(int y = ns->h-1; y>=0; y--) loop(x, ns->w)
        {
            uchar *nd = (uchar *)ns->pixels+(x+y*ns->w)*3;
            fputc(nd[2], f);
            fputc(255-nd[1], f);
            fputc(nd[0], f);
        };
        fclose(f);
    };
    if(ns) SDL_FreeSurface(ns);
};

void mergenormalmaps(char *heightfile, char *normalfile)    // BGR (tga) -> BGR (tga) (SDL loads TGA as BGR!)
{
    SDL_Surface *hs = IMG_Load(heightfile);
    SDL_Surface *ns = IMG_Load(normalfile);
    if(hs && ns)
    {
        uchar def_n[] = { 255, 128, 128 };
        FILE *f = fopen(normalfile, "wb");
        if(f)
        {
            writetgaheader(f, ns, 24);
            for(int y = ns->h-1; y>=0; y--) loop(x, ns->w)
            {
                int off = (x+y*ns->w)*3;
                uchar *hd = hs ? (uchar *)hs->pixels+off : def_n;
                uchar *nd = ns ? (uchar *)ns->pixels+off : def_n;
                #define S(x) x/255.0f*2-1
                vec n(S(nd[0]), S(nd[1]), S(nd[2]));
                vec h(S(hd[0]), S(hd[1]), S(hd[2]));
                n.mul(2).add(h).normalize().add(1).div(2).mul(255);
                uchar o[3] = { (uchar)n.x, (uchar)n.y, (uchar)n.z };
                fwrite(o, 3, 1, f);
                #undef S
            };
            fclose(f);
        };
    };
    if(hs) SDL_FreeSurface(hs);
    if(ns) SDL_FreeSurface(ns);
};

COMMAND(flipnormalmapy, "ss");
COMMAND(mergenormalmaps, "sss");

void render2dbox(vec &o, float x, float y, float z)
{
    glBegin(GL_POLYGON);
    glVertex3f(o.x, o.y, o.z);
    glVertex3f(o.x, o.y, o.z+z);
    glVertex3f(o.x+x, o.y+y, o.z+z);
    glVertex3f(o.x+x, o.y+y, o.z);
    glEnd();
};

void render3dbox(vec &o, float tofloor, float toceil, float xradius, float yradius)
{
    if(yradius<=0) yradius = xradius;
    vec c = o;
    c.sub(vec(xradius, yradius, tofloor));
    float xsz = xradius*2, ysz = yradius*2;
    float h = tofloor+toceil;
    notextureshader->set();
    glColor3f(1, 1, 1);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDepthMask(GL_FALSE);
    render2dbox(c, xsz, 0, h);
    render2dbox(c, 0, ysz, h);
    c.add(vec(xsz, ysz, 0));
    render2dbox(c, -xsz, 0, h);
    render2dbox(c, 0, -ysz, h);
    glDepthMask(GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_CULL_FACE);
    xtraverts += 16;
};

void box(block &b, float z1, float z2, float z3, float z4)
{
    glBegin(GL_POLYGON);
    glVertex3f((float)b.x,      (float)b.y,      z1);
    glVertex3f((float)b.x+b.xs, (float)b.y,      z2);
    glVertex3f((float)b.x+b.xs, (float)b.y+b.ys, z3);
    glVertex3f((float)b.x,      (float)b.y+b.ys, z4);
    glEnd();
    xtraverts += 4;
};

void dot(int x, int y, float z)
{
    const float DOF = 0.1f;
    glBegin(GL_POLYGON);
    glVertex3f(x-DOF, y-DOF, (float)z);
    glVertex3f(x+DOF, y-DOF, (float)z);
    glVertex3f(x+DOF, y+DOF, (float)z);
    glVertex3f(x-DOF, y+DOF, (float)z);
    glEnd();
    xtraverts += 4;
};

void blendbox(int x1, int y1, int x2, int y2, bool border)
{
    notextureshader->set();

    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glBegin(GL_QUADS);
    if(border) glColor3d(0.5, 0.3, 0.4);
    else glColor3d(1.0, 1.0, 1.0);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1);
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_POLYGON);
    glColor3d(0.2, 0.7, 0.4);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1);
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    xtraverts += 8;
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);

    defaultshader->set();
};

struct sphere { vec o; float size, max; int type; };

vector<sphere> spheres;
Texture *expltex = NULL;
Shader *explshader = NULL;

VARP(damagespherefactor, 0, 100, 200);

void newsphere(vec &o, float max, int type)
{
    if(damagespherefactor<=10) return;
    sphere p;
    p.o = o;
    p.max = max*damagespherefactor/100;
    p.size = 4;
    p.type = type;
    spheres.add(p);
};

extern float reflecting;

void renderspheres(int time)
{
    if(spheres.empty()) return;

    static struct spheretype { float r, g, b; } spheretypes[2] =
    {
        { 1.0f, 0.5f, 0.5f },
        { 0.9f, 1.0f, 0.5f },
    };

    if(!explshader) explshader = lookupshaderbyname("explosion");
    if(!expltex) expltex = textureload("data/explosion.jpg");
    explshader->set();
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, expltex->gl);
    loopv(spheres)
    {
        sphere &p = spheres[i];
        spheretype &pt = spheretypes[p.type];
        float size = p.size/p.max;
        if(renderpath!=R_FIXEDFUNCTION)
        {
            glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, p.o.x, p.o.y, p.o.z, 0);
            glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 1, size, p.size, p.max, 0);
        };
        glColor4f(pt.r, pt.g, pt.b, 1.0f-size*size);
        glPushMatrix();
        glTranslatef(p.o.x, p.o.y, p.o.z);
        glRotatef(lastmillis/5.0f, 1, 1, 1);
        glScalef(p.size, p.size, p.size);
        glCallList(1);
        if(renderpath!=R_FIXEDFUNCTION) glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, p.o.z, p.o.x, p.o.y, 0);
        glScalef(0.8f, 0.8f, 0.8f);
        glCallList(1);
        glPopMatrix();
        xtraverts += 12*6*2;
        if(!reflecting)
        {
            if(p.size>p.max)
            {
                spheres.remove(i);
            }
            else
            {
                p.size += time/25.0f;
            };
        };
    };
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    defaultshader->set();
};

string closeent, fullentname;

char *entname(entity &e)
{
    s_strcpy(fullentname, "@");
    s_strcat(fullentname, et->entname(e.type));
    const char *einfo = et->entnameinfo(e);
    if(*einfo)
    {
        s_strcat(fullentname, ": ");
        s_strcat(fullentname, einfo);
    };
    return fullentname;
};

extern int closestent();

void renderents()       // show sparkly thingies for map entities in edit mode
{
    closeent[0] = 0;
    if(!editmode) return;
    const vector<extentity *> &ents = et->getents();

    bool implicit = !haveselent();
    entity *c = NULL;
    int s;
    if(entgroup.length() > 1)
    {
        s_sprintf(closeent)("%d entities selected", entgroup.length());        
    }    
    else
    {
        if(implicit)
            s = closestent();
        else
            s = entgroup[0];
        if(s>=0)
        {
            c = ents[s];
            s_sprintf(closeent)("%s%s (%d, %d, %d, %d)", implicit ? "" : "\f0", entname(*c)+1, c->attr1, c->attr2, c->attr3, c->attr4);
            if(!implicit)
                c = NULL;
        };
    };

    loopv(ents)
    {
        entity &e = *ents[i];
        if(e.type==ET_EMPTY) continue;
        if(e.o.dist(camera1->o)<128)
        {
            particle_text(e.o, entname(e), &e==c ? 14 : 11, 1);
        };
        particle_splash(2, 2, 40, e.o);
    };
    loopv(entgroup)
    {
        entity &e = *ents[entgroup[i]];
        if(e.o.dist(camera1->o)<128)
        {
            particle_text(e.o, entname(e), 13, 1);
        };
    };
};

GLfloat mm[16];

vec worldpos, camright, camup;

void aimat()
{
    glGetFloatv(GL_MODELVIEW_MATRIX, mm);

    camright = vec(mm[0], mm[4], mm[8]);
    camup = vec(mm[1], mm[5], mm[9]);

    vec dir(0, 0, 0);
    vecfromyawpitch(camera1->yaw, camera1->pitch, 1, 0, dir, true);
    if(raycubepos(camera1->o, dir, worldpos, 0, RAY_CLIPMAT|RAY_SKIPFIRST) == -1)
        worldpos = dir.mul(10).add(camera1->o); //otherwise 3dgui won't work when outside of map
};

VARP(crosshairsize, 0, 15, 50);
VARP(cursorsize, 0, 30, 50);
VARP(damageblendfactor, 0, 300, 1000);

int dblend = 0;
void damageblend(int n) { dblend += n; };

VARP(hidestats, 0, 0, 1);
VARP(hidehud, 0, 0, 1);
VARP(crosshairfx, 0, 1, 1);

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
    aimat();
    renderents();

    if(editmode && !hidehud)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDepthMask(GL_FALSE);
        cursorupdate();
        glDepthMask(GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    };

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gettextres(w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);

    glEnable(GL_BLEND);

    if(dblend || underwater)
    {
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        glBegin(GL_QUADS);
        if(dblend) glColor3f(1.0f, 0.1f, 0.1f);
        else
        {
            getwatercolour(wcol);
            float maxc = max(wcol[0], max(wcol[1], wcol[2]));
            float wblend[3];
            loopi(3) wblend[i] = wcol[i] / min(32 + maxc*7/8, 255);
            glColor3fv(wblend);
            //glColor3f(0.1f, 0.5f, 1.0f);
        };
        glVertex2i(0, 0);
        glVertex2i(w, 0);
        glVertex2i(w, h);
        glVertex2i(0, h);
        glEnd();
        glDepthMask(GL_TRUE);
        dblend -= curtime*100/damageblendfactor;
        if(dblend<0) dblend = 0;
    };

    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    glLoadIdentity();
    glOrtho(0, w*3, h*3, 0, -1, 1);

    int abovegameplayhud = h*3*1650/1800-FONTH*3/2; // hack
    int hoff = abovegameplayhud - (editmode ? FONTH*4 : 0);

    char *command = getcurcommand();
    if(command) rendercommand(FONTH/2, hoff); else hoff += FONTH;

    if(!hidehud)
    {	
        bool windowhit = g3d_windowhit(true, false);
        if(/*!rendermenu(w, h) && */windowhit || player->state!=CS_SPECTATOR)
        {
            static Texture *cursor = NULL;
            if(!cursor) cursor = textureload("data/guicursor.png", true, false);
            
            glBlendFunc(GL_ONE, GL_ONE);
            glColor3f(1, 1, 1);
            float chsize = (float)(windowhit ? cursorsize : crosshairsize)*w/300;
            float x = w*1.5f - (windowhit ? 0 : chsize/2.0f);
            float y = h*1.5f - (windowhit ? 0 : chsize/2.0f);
            glBindTexture(GL_TEXTURE_2D, (windowhit ? cursor : crosshair)->gl);
            glBegin(GL_QUADS);
            glTexCoord2d(0.0, 0.0); glVertex2f(x,          y);
            glTexCoord2d(1.0, 0.0); glVertex2f(x + chsize, y);
            glTexCoord2d(1.0, 1.0); glVertex2f(x + chsize, y + chsize);
            glTexCoord2d(0.0, 1.0); glVertex2f(x,          y + chsize);
            glEnd();
        };

        /*int coff = */ renderconsole(w, h);
        // can render stuff below console output here        

        if(!hidestats)
        {
            //draw_textf("fps %d", 0, h*3-100, curfps);

            if(editmode)
            {
                draw_textf("cube %s%d", FONTH/2, abovegameplayhud-FONTH*3, selchildcount<0 ? "1/" : "", abs(selchildcount));
                draw_textf("wtr:%dk(%d%%) wvt:%dk(%d%%) evt:%dk eva:%dk", FONTH/2, abovegameplayhud-FONTH*2, wtris/1024, vtris*100/max(wtris, 1), wverts/1024, vverts*100/max(wverts, 1), xtraverts/1024, xtravertsva/1024);
                draw_textf("ond:%d va:%d gl:%d oq:%d lm:%d, rp:%d", FONTH/2, abovegameplayhud-FONTH, allocnodes*8, allocva, glde, getnumqueries(), lightmaps.length(), rplanes);
            };
        };

        if(closeent[0]) draw_text(closeent, FONTH/2, abovegameplayhud);

        cl->gameplayhud(w, h);
        render_texture_panel(w, h);
    };

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
};

