// renderparticles.cpp

#include "pch.h"
#include "engine.h"

#define MAXPARTYPES 21

struct particle
{
    vec o, d;
    int fade, type;
    int millis;
    particle *next;
    union
    {
        char *text;         // will call delete[] on this only if it starts with an @
        int val;
    };
};

particle *parlist[MAXPARTYPES], *parempty = NULL;

VARP(particlesize, 20, 100, 500);

Texture *parttexs[6];

void particleinit()
{
    parttexs[0] = textureload("data/martin/base.png");
    parttexs[1] = textureload("data/martin/ball1.png");
    parttexs[2] = textureload("data/martin/smoke.png");
    parttexs[3] = textureload("data/martin/ball2.png");
    parttexs[4] = textureload("data/martin/ball3.png");
    parttexs[5] = textureload("data/flare.jpg");
    loopi(MAXPARTYPES) parlist[i] = NULL;
};

particle *newparticle(const vec &o, const vec &d, int fade, int type)
{
    if(!parempty)
    {
        particle *ps = new particle[256];
        loopi(256)
        {
            ps[i].next = parempty;
            parempty = &ps[i];
        };
    };
    particle *p = parempty;
    parempty = p->next;
    p->o = o;
    p->d = d;
    p->fade = fade;
    p->type = type;
    p->millis = lastmillis;
    p->next = parlist[type];
    p->text = NULL;
    parlist[type] = p;
    return p;
};

enum
{
    PT_PART = 0,
    PT_FLARE,
    PT_EDIT,
    PT_TEXT,
    PT_TEXTUP,
    PT_METER,
    PT_METERVS
};

extern float reflecting;

void render_particles(int time)
{
    static struct parttype { int type; uchar r, g, b; int gr, tex; float sz; } parttypes[MAXPARTYPES] =
    {
        { 0,          180, 155, 75,  2,  0, 0.24f }, // yellow: sparks 
        { 0,          125, 125, 125, 20, 2, 0.6f },  // grey:   small smoke
        { 0,          50, 50, 255,   20, 0, 0.32f }, // blue:   edit mode entities
        { 0,          255, 25, 25,   1,  2, 0.24f }, // red:    blood spats
        { 0,          255, 200, 200, 20, 1, 4.8f  }, // yellow: fireball1
        { 0,          125, 125, 125, 20, 2, 2.4f  }, // grey:   big smoke   
        { 0,          255, 255, 255, 20, 3, 4.8f  }, // blue:   fireball2
        { 0,          255, 255, 255, 20, 4, 4.8f  }, // green:  big fireball3
        { PT_TEXTUP,  255, 75, 25,   -8, -1, 4.0f }, // 8 TEXT RED
        { PT_TEXTUP,  50, 255, 100,  -8, -1, 4.0f }, // 9 TEXT GREEN
        { PT_FLARE,   255, 0, 0, 0,  5, 0.28f }, // 10 yellow flare
        { PT_TEXT,    30, 200, 80,   0, -1, 2.0f },  // 11 TEXT DARKGREEN, SMALL, NON-MOVING
        { 0,          255, 255, 255, 20, 4, 2.0f },  // 12 green small fireball3
        { PT_TEXT,    255, 75, 25,   0, -1, 2.0f },  // 13 TEXT RED, SMALL, NON-MOVING
        { PT_TEXT,    180, 180, 180, 0, -1, 2.0f },  // 14 TEXT GREY, SMALL, NON-MOVING
        { PT_TEXTUP,  255, 200, 100, -8, -1, 4.0f }, // 15 TEXT YELLOW
        { PT_TEXT,    100, 150, 255, 0, -1, 2.0f },  // 16 TEXT BLUE, SMALL, NON-MOVING
        { PT_METER,   255, 25, 25,   0, -1, 2.0f },  // 17 METER RED, SMALL, NON-MOVING
        { PT_METER,   50, 50, 255,   0, -1, 2.0f },  // 18 METER BLUE, SMALL, NON-MOVING
        { PT_METERVS, 255, 25, 25,   0, -1, 2.0f },  // 19 METER RED vs. BLUE, SMALL, NON-MOVING
        { PT_METERVS, 50, 50, 255,   0, -1, 2.0f },  // 20 METER BLUE vs. RED, SMALL, NON-MOVING
    };
       
    bool enabled = false;
    loopi(MAXPARTYPES) if(parlist[i])
    {
        parttype &pt = parttypes[i];
        float sz = pt.sz*particlesize/100.0f; 

        if(!enabled)
        {
            enabled = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        };

        if(pt.tex>=0)
        {
            glBindTexture(GL_TEXTURE_2D, parttexs[pt.tex]->gl);
            glBegin(GL_QUADS);
            glColor3ub(pt.r, pt.g, pt.b);
        };
        
        for(particle *p, **pp = &parlist[i]; (p = *pp);)
        {   
            int blend = p->fade*255/(lastmillis-p->millis+p->fade);
            if(pt.tex>=0)  
            {    
                if(pt.type==PT_FLARE)   // flares
                {
                    glColor4ub(pt.r, pt.g, pt.b, blend);
					vec dir1 = p->d, dir2 = p->d, c1, c2;
					dir1.sub(p->o);
					dir2.sub(camera1->o);
					c1.cross(dir2, dir1).normalize().mul(sz);
					c2.cross(dir1, dir2).normalize().mul(sz);
                    glTexCoord2f(0.0, 0.0); glVertex3f(p->d.x+c1.x, p->d.y+c1.y, p->d.z+c1.z);
                    glTexCoord2f(0.0, 1.0); glVertex3f(p->d.x+c2.x, p->d.y+c2.y, p->d.z+c2.z);
                    glTexCoord2f(1.0, 1.0); glVertex3f(p->o.x+c2.x, p->o.y+c2.y, p->o.z+c2.z);
                    glTexCoord2f(1.0, 0.0); glVertex3f(p->o.x+c1.x, p->o.y+c1.y, p->o.z+c1.z);
                }
                else        // regular particles
                {
                    glTexCoord2f(0.0, 1.0); glVertex3f(p->o.x+(-camright.x+camup.x)*sz, p->o.y+(-camright.y+camup.y)*sz, p->o.z+(-camright.z+camup.z)*sz);
                    glTexCoord2f(1.0, 1.0); glVertex3f(p->o.x+( camright.x+camup.x)*sz, p->o.y+( camright.y+camup.y)*sz, p->o.z+( camright.z+camup.z)*sz);
                    glTexCoord2f(1.0, 0.0); glVertex3f(p->o.x+( camright.x-camup.x)*sz, p->o.y+( camright.y-camup.y)*sz, p->o.z+( camright.z-camup.z)*sz);
                    glTexCoord2f(0.0, 0.0); glVertex3f(p->o.x+(-camright.x-camup.x)*sz, p->o.y+(-camright.y-camup.y)*sz, p->o.z+(-camright.z-camup.z)*sz);
                };
                if(pt.type!=PT_EDIT) xtraverts += 4;
            }
            else            // text
            {
                glPushMatrix();
                glTranslatef(p->o.x, p->o.y, p->o.z);
                glRotatef(camera1->yaw-180, 0, 0, 1);
                glRotatef(camera1->pitch-90, 1, 0, 0);
                float scale = pt.sz/80.0f;
                glScalef(-scale, scale, -scale);
                if(pt.type==PT_METER || pt.type==PT_METERVS)
                {
                    float right = 8*FONTH, left = p->val*right/100.0f;
                    glDisable(GL_BLEND);
                    glDisable(GL_TEXTURE_2D);
                    notextureshader->set();

                    glTranslatef(-right/2.0f, 0, 0);
                
                    glBegin(GL_QUADS);
                    glColor3ub(pt.r, pt.g, pt.b);
                    glVertex2f(0, 0);
                    glVertex2f(left, 0);
                    glVertex2f(left, FONTH);
                    glVertex2f(0, FONTH);

                    if(pt.type==PT_METERVS) glColor3ub(parttypes[i-1].r, parttypes[i-1].g, parttypes[i-1].b);
                    else glColor3ub(0, 0, 0);
                    glVertex2f(left, 0);
                    glVertex2f(right, 0);
                    glVertex2f(right, FONTH);
                    glVertex2f(left, FONTH);
                    glEnd();

                    defaultshader->set();
                    glEnable(GL_TEXTURE_2D);
                    glEnable(GL_BLEND);
                }
                else
                {
                    char *t = p->text+(p->text[0]=='@');
                    float xoff = -text_width(t)/2;
                    float yoff = 0;
                    if(pt.type==PT_TEXTUP) { xoff += detrnd((size_t)p, 100)-50; yoff -= detrnd((size_t)p, 101); } else blend = 255;
                    glTranslatef(xoff, yoff, 50);
                    draw_text(t, 0, 0, pt.r, pt.g, pt.b, blend);
                };
                glPopMatrix();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            };

            if(!reflecting && (p->fade -= time)<=0)
            {
                *pp = p->next;
                p->next = parempty;
                if((pt.type==PT_TEXT || pt.type==PT_TEXTUP) && p->text && p->text[0]=='@') delete[] p->text;
                parempty = p;
            }
            else
            {
                if(!reflecting && pt.gr)
                {
                    p->o.z -= ((lastmillis-p->millis)/3.0f)*curtime/(pt.gr*2500);
                    vec a = p->d;
                    a.mul((float)time);
                    a.div(5000.0f);
                    p->o.add(a);
                };
                pp = &p->next;
            };
        };
        
        if(pt.tex>=0) glEnd();
    };

    if(enabled)
    {        
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    };
};

VARP(maxparticledistance, 256, 512, 4096);

void particle_splash(int type, int num, int fade, const vec &p)
{
    if(camera1->o.dist(p)>maxparticledistance) return; 
    loopi(num)
    {
        const int radius = type==5 ? 50 : 150;
        int x, y, z;
        do
        {
            x = rnd(radius*2)-radius;
            y = rnd(radius*2)-radius;
            z = rnd(radius*2)-radius;
        }
        while(x*x+y*y+z*z>radius*radius);
    	vec tmp =  vec((float)x, (float)y, (float)z);
        newparticle(p, tmp, rnd(fade*3)+1, type);
    };
};

void particle_trail(int type, int fade, const vec &s, const vec &e)
{
    vec v;
    float d = e.dist(s, v);
    v.div(d*2);
    vec p = s;
    loopi((int)d*2)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        newparticle(p, tmp, rnd(fade)+fade, type);
    };
};

VARP(particletext, 0, 1, 1);

void particle_text(const vec &s, char *t, int type, int fade)
{
    if(!particletext) return;
    if(t[0]=='@') t = newstring(t);
    particle *p = newparticle(s, vec(0, 0, 1), fade, type);
    p->text = t;
};

void particle_meter(const vec &s, int val, int type, int fade)
{
    particle *p = newparticle(s, vec(0, 0, 1), fade, type);
    p->val = val;
};

void particle_flare(const vec &p, const vec &dest, int fade)
{
    newparticle(p, dest, fade, 10);
};

