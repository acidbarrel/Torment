// creates multiple gui windows that float inside the 3d world

// special feature is that its mostly *modeless*: you can use this menu while playing, without turning menus on or off
// implementationwise, it is *stateless*: it keeps no internal gui structure, hit tests are instant, usage & implementation is greatly simplified

#include "pch.h"
#include "engine.h"

#define TILE 1

static bool layoutpass, actionon = false;
static int mousebuttons = 0;
static g3d_gui *windowhit = NULL;

#define SHADOW 4
#define ICON_SIZE (FONTH-SHADOW)
#define SKIN_W 256
#define SKIN_H 128
#define SKIN_SCALE 4
#define INSERT (3*SKIN_SCALE)

VARP(guiautotab, FONTH*6, FONTH*16, FONTH*40);

struct gui : g3d_gui
{
    struct list
    {
        int parent, w, h;
    };

    int nextlist;

    static vector<list> lists;
    static float hitx, hity;
    static int curdepth, curlist, xsize, ysize, curx, cury;

    static void reset()
    {
        lists.setsize(0);
    };

    static int ty, tx, tpos, *tcurrent, tcolor; //tracking tab size and position since uses different layout method...

    void autotab() 
    { 
        if(tcurrent)
        {
            if(layoutpass && !tpos) tcurrent = NULL; //disable tabs because you didn't start with one
            if(!curdepth && (layoutpass ? 0 : cury) + ysize > guiautotab) tab(NULL, tcolor); 
        };
    };

    bool visible() { return (!tcurrent || tpos==*tcurrent) && !layoutpass; };

    //tab is always at top of page
    void tab(const char *name, int color) 
    {
        if(curdepth != 0) return;
        tcolor = color;
        tpos++; 
        s_sprintfd(title)("%d", tpos);
        if(!name) name = title;
        int w = text_width(name) - 2*INSERT;
        if(layoutpass) 
        {  
            ty = max(ty, ysize); 
            ysize = 0;
        }
        else 
        {	
            cury = -ysize;
            int h = FONTH-2*INSERT,
                x1 = curx + tx,
                x2 = x1 + w + ((skinx[3]-skinx[2]) + (skinx[5]-skinx[4]))*SKIN_SCALE,
                y1 = cury - ((skiny[5]-skiny[1])-(skiny[3]-skiny[2]))*SKIN_SCALE-h,
                y2 = cury;
            bool hit = tcurrent && windowhit==this && hitx>=x1 && hity>=y1 && hitx<x2 && hity<y2;
            if(hit) 
            {	
                *tcurrent = tpos; //roll-over to switch tab
                color = 0xFF0000;
            };
            
            skin_(x1-skinx[visible()?2:6]*SKIN_SCALE, y1-skiny[1]*SKIN_SCALE, w, h, visible()?10:19, 9);
            text_(name, x1 + (skinx[3]-skinx[2])*SKIN_SCALE - INSERT, y1 + (skiny[2]-skiny[1])*SKIN_SCALE - INSERT, color, visible());
        };
        tx += w + ((skinx[5]-skinx[4]) + (skinx[3]-skinx[2]))*SKIN_SCALE; 
    };

    bool ishorizontal() const { return curdepth&1; };
    bool isvertical() const { return !ishorizontal(); };

    void pushlist()
    {	
        if(layoutpass)
        {
            if(curlist>=0)
            {
                lists[curlist].w = xsize;
                lists[curlist].h = ysize;
            };
            list &l = lists.add();
            l.parent = curlist;
            curlist = lists.length()-1;
            xsize = ysize = 0;
        }
        else
        {
            curlist = nextlist++;
            xsize = lists[curlist].w;
            ysize = lists[curlist].h;
        };
        curdepth++;		
    };

    void poplist()
    {
        list &l = lists[curlist];
        if(layoutpass)
        {
            l.w = xsize;
            l.h = ysize;
        };
        curlist = l.parent;
        curdepth--;
        if(curlist>=0)
        {
            xsize = lists[curlist].w;
            ysize = lists[curlist].h;
            if(ishorizontal()) cury -= l.h;
            else curx -= l.w;
            layout(l.w, l.h);
        };
    };

    int text  (const char *text, int color, const char *icon) { autotab(); return button_(text, color, icon, false, false); };
    int button(const char *text, int color, const char *icon) { autotab(); return button_(text, color, icon, true, false); };
    int title (const char *text, int color, const char *icon) { autotab(); return button_(text, color, icon, false, true); };

    void separator() { autotab(); line_(5); };
    void progress(float percent) { autotab(); line_(FONTH*2/5, percent); };

    //use to set min size (useful when you have progress bars)
    void strut(int size) { layout(isvertical() ? size*FONTH : 0, isvertical() ? 0 : size*FONTH); };

    int layout(int w, int h)
    {
        if(layoutpass)
        {
            if(ishorizontal())
            {
                xsize += w;
                ysize = max(ysize, h);
            }
            else
            {
                xsize = max(xsize, w);
                ysize += h;
            };
            return 0;
        }
        else
        {
            bool hit = ishit(w, h);
            if(ishorizontal()) curx += w;
            else cury += h;
            return (hit && visible()) ? mousebuttons|G3D_ROLLOVER : 0;
        };
    };

    bool ishit(int w, int h, int x = curx, int y = cury)
    {
        if(ishorizontal()) h = ysize;
        else w = xsize;
        return windowhit==this && hitx>=x && hity>=y && hitx<x+w && hity<y+h;
    };

    //one day to replace render_texture_panel()...?
    int image(const char *path, float scale, bool overlaid)
    {
        Texture *t = textureload(path, false, true, false);
        if(t==crosshair) return 0;
        autotab();
        if(scale==0) scale = 1;
        int size = (int)(scale*2*FONTH)-SHADOW;
        if(visible()) icon_(t, overlaid, curx, cury, size, ishit(size+SHADOW, size+SHADOW));
        return layout(size+SHADOW, size+SHADOW);
    };

    void slider(int &val, int vmin, int vmax, int color)
    {	
        autotab();
        int x = curx;
        int y = cury;
        line_(10);
        if(visible()) 
        {
            s_sprintfd(label)("%d", val);
            int w = text_width(label);
        
            bool hit;
            int px, py;
            if(ishorizontal()) 
            {
                hit = ishit(FONTH, ysize, x, y);
                px = x + (FONTH-w)/2;
                py = y + (ysize-FONTH) - ((ysize-FONTH)*(val-vmin))/(vmax-vmin); //zero at the bottom
            }
            else
            {
                hit = ishit(xsize, FONTH, x, y);
                px = x + ((xsize-w)*(val-vmin))/(vmax-vmin);
                py = y;
            };
        
            if(hit) color = 0xFF0000;
            text_(label, px, py, color, hit && actionon);
            if(hit && actionon) 
            {
                int vnew = 1+vmax-vmin;
                if(ishorizontal()) vnew = int(vnew*(y+ysize-hity)/ysize);
                else vnew = int(vnew*(hitx-x)/xsize);
                vnew += vmin;
                if(vnew != val) val = vnew;
            };
        };
    };

    void rect_(float x, float y, float w, float h, int usetc = -1) 
    {
        GLint tc[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        if(usetc>=0) glTexCoord2iv(tc[usetc]); 
        glVertex2f(x, y);
        if(usetc>=0) glTexCoord2iv(tc[(usetc+1)%4]);
        glVertex2f(x + w, y);
        if(usetc>=0) glTexCoord2iv(tc[(usetc+2)%4]);
        glVertex2f(x + w, y + h);
        if(usetc>=0) glTexCoord2iv(tc[(usetc+3)%4]);
        glVertex2f(x, y + h);
        xtraverts += 4;
    };

    void text_(const char *text, int x, int y, int color, bool shadow) 
    {
        if(shadow) draw_text(text, x+SHADOW, y+SHADOW, 0x00, 0x00, 0x00, 0xC0);
        draw_text(text, x, y, color>>16, (color>>8)&0xFF, color&0xFF);
    };

    void icon_(Texture *t, bool overlaid, int x, int y, int size, bool hit) 
    {
        float scale = float(size)/max(t->xs, t->ys); //scale and preserve aspect ratio
        float xs = t->xs*scale;
        float ys = t->ys*scale;
        float xo = x + (size-xs)/2;
        float yo = y + (size-ys)/2;
        if(hit && actionon) 
        {
            glDisable(GL_TEXTURE_2D);
            notextureshader->set();
            glColor4ub(0x00, 0x00, 0x00, 0xC0);
            glBegin(GL_QUADS);
            rect_(xo+SHADOW, yo+SHADOW, xs, ys);
            glEnd();
            glEnable(GL_TEXTURE_2D);
            defaultshader->set();	
        };
        loopi(overlaid ? 2 : 1)
        {
            if(i==1)
            {
                if(!overlaytex) overlaytex = textureload("data/guioverlay.png");
                t = overlaytex;
                hit = false;
            };
            glColor4ub(0xFF, hit?0x80:0xFF, hit?0x80:0xFF, 0xFF);
            glBindTexture(GL_TEXTURE_2D, t->gl);
            glBegin(GL_QUADS);
            rect_(xo, yo, xs, ys, 0);
            glEnd();
            xtraverts += 4;
        };
    };

    void line_(int size, float percent = 1.0f)
    {		
        if(visible())
        {
            if(!slidertex) slidertex = textureload("data/guislider.png");
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, slidertex->gl);
            glBegin(GL_QUADS);
            if(percent < 0.99f) 
            {
                glColor4ub(0xFF, 0xFF, 0xFF, 0x60);
                if(ishorizontal()) 
                    rect_(curx + FONTH/2 - size, cury, size*2, ysize, 0);
                else
                    rect_(curx, cury + FONTH/2 - size, xsize, size*2, 1);
            };
            glColor4ub(0xFF, 0xFF, 0xFF, 0xFF);
            if(ishorizontal()) 
                rect_(curx + FONTH/2 - size, cury + ysize*(1-percent), size*2, ysize*percent, 0);
            else 
                rect_(curx, cury + FONTH/2 - size, xsize*percent, size*2, 1);
            glEnd();
        };
        layout(ishorizontal() ? FONTH : 0, ishorizontal() ? 0 : FONTH);
    };

    int button_(const char *text, int color, const char *icon, bool clickable, bool center)
    {
        const int padding = 10;
        int w = 0;
        if(icon) w += ICON_SIZE;
        if(icon && text) w += padding;
        if(text) w += text_width(text);
    
        if(visible())
        {
            bool hit = ishit(w, FONTH);
            if(hit && clickable) color = 0xFF0000;	
            int x = curx;	
            if(isvertical() && center) x += (xsize-w)/2;
        
            if(icon)
            {
                s_sprintfd(tname)("packages/icons/%s.jpg", icon);
                icon_(textureload(tname), false, x, cury, ICON_SIZE, clickable && hit);
                x += ICON_SIZE;
            };
            if(icon && text) x += padding;
            if(text) text_(text, x, cury, color, center || (hit && clickable && actionon));
        };
        return layout(w, FONTH);
    };

    static Texture *skintex, *overlaytex, *slidertex;
    static const int skinx[], skiny[];
    static const struct patch { ushort left, right, top, bottom; uchar flags; } patches[];

    void skin_(int x, int y, int gapw, int gaph, int start, int n)//int vleft, int vright, int vtop, int vbottom, int start, int n) 
    {
        if(!skintex) skintex = textureload("data/guiskin.png");
        glBindTexture(GL_TEXTURE_2D, skintex->gl);
        int gapx1 = INT_MAX, gapy1 = INT_MAX, gapx2 = INT_MAX, gapy2 = INT_MAX;
        float wscale = 1.0f/(SKIN_W*SKIN_SCALE), hscale = 1.0f/(SKIN_H*SKIN_SCALE);
        loopj(2)
        {	
            glDepthFunc(j?GL_LEQUAL:GL_GREATER);
            glColor4f(j?light.x:1.0f, j?light.y:1.0f, j?light.z:1.0f, j?0.80f:0.35f); //ghost when its behind something in depth
            glBegin(GL_QUADS);
            loopi(n)
            {
                const patch &p = patches[start+i];
                int left = skinx[p.left]*SKIN_SCALE, right = skinx[p.right]*SKIN_SCALE,
                    top = skiny[p.top]*SKIN_SCALE, bottom = skiny[p.bottom]*SKIN_SCALE;
                float tleft = left*wscale, tright = right*wscale,
                      ttop = top*hscale, tbottom = bottom*hscale;
                if(p.flags&0x1)
                {
                    gapx1 = left;
                    gapx2 = right;
#ifndef TILE
                    right = left + gapw;
                    if(gapw<gapx2-gapx1) tright = right*wscale;
#endif
                }
                else if(left >= gapx2)
                {
                    left += gapw - (gapx2-gapx1);
                    right += gapw - (gapx2-gapx1);
                };
                if(p.flags&0x10)
                {
                    gapy1 = top;
                    gapy2 = bottom;
#ifndef TILE
                    bottom = top + gaph;
                    if(gaph<gapy2-gapy1) tbottom = bottom*hscale;
#endif
                }
                else if(top >= gapy2)
                {
                    top += gaph - (gapy2-gapy1);
                    bottom += gaph - (gapy2-gapy1);
                };
               
#ifdef TILE
                //multiple tiled quads if necessary rather than a single stretched one
                int ystep = bottom-top;
                int yo = y+top;
                while(ystep > 0) {
                    if(p.flags&0x10 && yo+ystep-(y+top) > gaph) 
                    {
                        ystep = gaph+y+top-yo;
                        tbottom = ttop+ystep*hscale;
                    };
                    int xstep = right-left;
                    int xo = x+left;
                    float tright2 = tright;
                    while(xstep > 0) {
                        if(p.flags&0x01 && xo+xstep-(x+left) > gapw) 
                        {
                            xstep = gapw+x+left-xo; 
                            tright = tleft+xstep*wscale;
                        };
                        glTexCoord2f(tleft,  ttop);    glVertex2i(xo,       yo);
                        glTexCoord2f(tright, ttop);    glVertex2i(xo+xstep, yo);
                        glTexCoord2f(tright, tbottom); glVertex2i(xo+xstep, yo+ystep);
                        glTexCoord2f(tleft,  tbottom); glVertex2i(xo,       yo+ystep);
                        xtraverts += 4;
                        if(!(p.flags&0x01)) break;
                        xo += xstep;
                    };
                    tright = tright2;
                    if(!(p.flags&0x10)) break;
                    yo += ystep;
                };
#else
                if(left==right || top==bottom) continue;
                glTexCoord2f(tleft, ttop); glVertex2i(x+left, y+top);
                glTexCoord2f(tright, ttop); glVertex2i(x+right, y+top);
                glTexCoord2f(tright, tbottom); glVertex2i(x+right, y+bottom);
                glTexCoord2f(tleft, tbottom); glVertex2i(x+left, y+bottom);
                xtraverts += 4;
#endif
            };
            glEnd();
        };
        glDepthFunc(GL_ALWAYS);
    }; 

    vec origin;
    float dist;
    g3d_callback *cb;

    static float scale;
    static bool passthrough;
    static vec light;

    void start(int starttime, float basescale, int *tab, bool allowinput)
    {	
        scale = basescale*min((lastmillis-starttime)/300.0f, 1.0f);
        passthrough = scale<basescale || !allowinput;
        curdepth = -1;
        curlist = -1;
        tpos = 0;
        tx = 0;
        ty = 0;
        tcurrent = tab;
        tcolor = 0xFFFFFF;
        pushlist();
        if(layoutpass) nextlist = curlist;
        else
        {
            if(tcurrent && !*tcurrent) tcurrent = NULL;
            cury = -ysize; 
            curx = -xsize/2;

            float yaw = atan2f(origin.y-camera1->o.y, origin.x-camera1->o.x) - 90*RAD;
            glPushMatrix();
            glTranslatef(origin.x, origin.y, origin.z);
            glRotatef(yaw/RAD, 0, 0, 1); 
            glRotatef(-90, 1, 0, 0);
            glScalef(-scale, scale, scale);
        
            vec dir;
            lightreaching(origin, light, dir, 0, 0.5f); 
            float intensity = vec(yaw, 0.0f).dot(dir);
            light.mul(1.0f + max(intensity, 0));
       
            skin_(curx-skinx[2]*SKIN_SCALE, cury-skiny[5]*SKIN_SCALE, xsize, ysize, 0, 9);
            if(!tcurrent) skin_(curx-skinx[5]*SKIN_SCALE, cury-skiny[5]*SKIN_SCALE, xsize, 0, 9, 1);
        };
    };

    void end()
    {
        if(layoutpass)
        {	
            xsize = max(tx, xsize);
            ysize = max(ty, ysize);
            ysize = max(ysize, (skiny[6]-skiny[5])*SKIN_SCALE);
            if(tcurrent) *tcurrent = max(1, min(*tcurrent, tpos));
            if(!windowhit && !passthrough)
            {
                vec planenormal = vec(origin).sub(camera1->o).set(2, 0).normalize(), intersectionpoint;
                int intersects = intersect_plane_line(camera1->o, worldpos, origin, planenormal, intersectionpoint);
                vec intersectionvec = vec(intersectionpoint).sub(origin), xaxis(-planenormal.y, planenormal.x, 0);
                hitx = xaxis.dot(intersectionvec)/scale;
                hity = -intersectionvec.z/scale;
                if(intersects>=INTERSECT_MIDDLE && hitx>=-xsize/2 && hitx<=xsize/2 && hity<=0)
                {
                    if(hity>=-ysize || (tcurrent && hity>=-ysize-(FONTH-2*INSERT)-((skiny[5]-skiny[1])-(skiny[3]-skiny[2]))*SKIN_SCALE && hitx<=tx-xsize/2))
                        windowhit = this;
                };
            };
        }
        else
        {
#ifdef TILE
            if(tcurrent && tx<xsize) skin_(curx+tx-skinx[5]*SKIN_SCALE, -ysize-skiny[5]*SKIN_SCALE, xsize-tx, FONTH, 9, 1);
#else
            if(tcurrent && tx<xsize) skin_(curx+tx-skinx[5]*SKIN_SCALE, -ysize-skiny[5]*SKIN_SCALE, xsize-tx, 0, 9, 1);
#endif
            glPopMatrix();
        };
        poplist();
    };
};

Texture *gui::skintex = NULL, *gui::overlaytex = NULL, *gui::slidertex = NULL;

//chop skin into a grid
const int gui::skiny[] = {0, 7, 21, 34, 48, 56, 104, 111, 128},
          gui::skinx[] = {0, 11, 23, 37, 105, 119, 137, 151, 215, 229, 256};
//Note: skinx[3]-skinx[2] = skinx[7]-skinx[6]
//      skinx[5]-skinx[4] = skinx[9]-skinx[8]		 
const gui::patch gui::patches[] = 
{ //arguably this data can be compressed - it depends on what else needs to be skinned in the future
    {1,2,3,5,  0},    // body
    {2,9,4,5,  0x01},
    {9,10,3,5, 0},

    {1,2,5,6,  0x10},
    {2,9,5,6,  0x11},
    {9,10,5,6, 0x10},

    {1,2,6,8,  0},
    {2,9,6,8,  0x01},
    {9,10,6,8, 0},

    {5,6,3,4, 0x01}, // top

    {2,3,1,2, 0},    // selected tab
    {3,4,1,2, 0x01},
    {4,5,1,2, 0},
    {2,3,2,3, 0x10},
    {3,4,2,3, 0x11},
    {4,5,2,3, 0x10},
    {2,3,3,4, 0},
    {3,4,3,4, 0x01},
    {4,5,3,4, 0},

    {6,7,1,2, 0},    // deselected tab
    {7,8,1,2, 0x01},
    {8,9,1,2, 0},
    {6,7,2,3, 0x10},
    {7,8,2,3, 0x11},
    {8,9,2,3, 0x10},
    {6,7,3,4, 0},
    {7,8,3,4, 0x01},
    {8,9,3,4, 0},
};

vector<gui::list> gui::lists;
float gui::scale, gui::hitx, gui::hity;
bool gui::passthrough;
vec gui::light;
int gui::curdepth, gui::curlist, gui::xsize, gui::ysize, gui::curx, gui::cury;
int gui::ty, gui::tx, gui::tpos, *gui::tcurrent, gui::tcolor;
static vector<gui> guis;

void g3d_addgui(g3d_callback *cb, vec &origin)
{
    gui &g = guis.add();
    g.cb = cb;
    g.origin = origin;
    g.dist = camera1->o.dist(origin);    
};

int g3d_sort(gui *a, gui *b) { return (int)(a->dist>b->dist)*2-1; };

bool g3d_windowhit(bool on, bool act)
{
    extern int cleargui(int n);
    if(act) mousebuttons |= (actionon=on) ? G3D_DOWN : G3D_UP;
    else if(!on && windowhit) cleargui(1);
    return windowhit!=NULL;
};

void g3d_render()   
{
    glMatrixMode(GL_MODELVIEW);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    windowhit = NULL;
    if(actionon) mousebuttons |= G3D_PRESSED;
    gui::reset();
    guis.setsize(0);
    
    // call all places in the engine that may want to render a gui from here, they call g3d_addgui()
    g3d_mainmenu();
    cl->g3d_gamemenus();
    
    guis.sort(g3d_sort);
    
    layoutpass = true;
    loopv(guis) guis[i].cb->gui(guis[i], true);
    layoutpass = false;
    loopvrev(guis) guis[i].cb->gui(guis[i], false);
    
    mousebuttons = 0;
	
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
};
