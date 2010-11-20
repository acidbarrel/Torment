// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "pch.h"
#include "engine.h"

VARP(animationinterpolationtime, 0, 150, 1000);

model *loadingmodel = NULL;

#include "tristrip.h"
#include "vertmodel.h"
#include "md2.h"
#include "md3.h"

#define checkmdl if(!loadingmodel) { conoutf("not loading a model"); return; }

void mdlcullface(int *cullface)
{
    checkmdl;
    loadingmodel->cullface = *cullface!=0;
};

COMMAND(mdlcullface, "i");

void mdlcollide(int *collide)
{
    checkmdl;
    loadingmodel->collide = *collide!=0;
};

COMMAND(mdlcollide, "i");

void mdlspec(int *percent)
{
    checkmdl;
    float spec = 1.0f; 
    if(*percent>0) spec = *percent/100.0f;
    else if(*percent<0) spec = 0.0f;
    loadingmodel->spec = spec;
};

COMMAND(mdlspec, "i");

void mdlambient(int *percent)
{
    checkmdl;
    float ambient = 0.3f;
    if(*percent>0) ambient = *percent/100.0f;
    else if(*percent<0) ambient = 0.0f;
    loadingmodel->ambient = ambient;
};

COMMAND(mdlambient, "i");

void mdlshader(char *shader)
{
    checkmdl;
    loadingmodel->shader = lookupshaderbyname(shader);
};

COMMAND(mdlshader, "s");

void mdlscale(int *percent)
{
    checkmdl;
    float scale = 0.3f;
    if(*percent>0) scale = *percent/100.0f;
    else if(*percent<0) scale = 0.0f;
    loadingmodel->scale = scale;
};  

COMMAND(mdlscale, "i");

void mdltrans(float *x, float *y, float *z)
{
    checkmdl;
    loadingmodel->translate = vec(*x, *y, *z);
}; 

COMMAND(mdltrans, "fff");

void mdlshadow(int *shadow)
{
    checkmdl;
    loadingmodel->shadow = *shadow!=0;
};

COMMAND(mdlshadow, "i");

void mdlbb(float *rad, float *h, float *eyeheight)
{
    checkmdl;
    loadingmodel->collideradius = *rad;
    loadingmodel->collideheight = *h;
    loadingmodel->eyeheight = *eyeheight; 
};

COMMAND(mdlbb, "fff");

void mdlname()
{
    checkmdl;
    result(loadingmodel->name());
};

COMMAND(mdlname, "");

// mapmodels

vector<mapmodelinfo> mapmodels;

void mmodel(char *name, int *tex)
{
    mapmodelinfo &mmi = mapmodels.add();
    s_strcpy(mmi.name, name);
    mmi.tex = *tex;
    mmi.m = NULL;
};

void mapmodelcompat(int *rad, int *h, int *tex, char *name, char *shadow)
{
    mmodel(name, tex);
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return mapmodels.inrange(i) ? mapmodels[i] : *(mapmodelinfo *)0; };

COMMAND(mmodel, "si");
COMMANDN(mapmodel, mapmodelcompat, "iiiss");
COMMAND(mapmodelreset, "");

// model registry

hashtable<const char *, model *> mdllookup;

model *loadmodel(const char *name, int i)
{
    if(!name)
    {
        if(!mapmodels.inrange(i)) return NULL;
        mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m) return mmi.m;
        name = mmi.name;
    };
    model **mm = mdllookup.access(name);
    model *m;
    if(mm) m = *mm;
    else
    { 
        m = new md2(name);
        loadingmodel = m;
        if(!m->load())
        {
            delete m;
            m = new md3(name);
            loadingmodel = m;
            if(!m->load())
            {    
                delete m;
                loadingmodel = NULL;
                return NULL; 
            };
        };
        loadingmodel = NULL;
        mdllookup.access(m->name(), &m);
    };
    if(mapmodels.inrange(i) && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
};

void clear_mdls()
{
    enumerate(mdllookup, model *, m, delete m);
};

bool modeloccluded(const vec &center, float radius)
{
    int br = int(radius*2)+1;
    return bboccluded(ivec(int(center.x-radius), int(center.y-radius), int(center.z-radius)), ivec(br, br, br), worldroot, ivec(0, 0, 0), hdr.worldsize/2);
};

VARP(maxmodelradiusdistance, 10, 80, 1000);

extern float reflecting, refracting;
extern int waterfog, reflectdist;

VAR(showboundingbox, 0, 0, 2);

void rendermodel(vec &color, vec &dir, const char *mdl, int anim, int varseed, int tex, float x, float y, float z, float yaw, float pitch, float speed, int basetime, dynent *d, int cull, const char *vwepmdl)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    if(cull)
    {
        vec center;
        float radius = m->boundsphere(0/*frame*/, center);   // FIXME
        center.add(vec(x, y, z));
        if((cull&MDL_CULL_DIST) && center.dist(camera1->o)/radius>maxmodelradiusdistance) return;
        if(cull&MDL_CULL_VFC)
        {
            if(refracting)
            {
                if(center.z+radius<refracting-waterfog || center.z-radius>=refracting) return;
            }
            else if(reflecting && center.z+radius<=reflecting) return;
            if((reflecting || refracting) && center.dist(camera1->o)-radius>reflectdist) return;
            if(isvisiblesphere(radius, center) >= VFC_FOGGED) return;
        };
        if((cull&MDL_CULL_OCCLUDED) && modeloccluded(center, radius)) return;
    };
    if(showboundingbox)
    {
        if(d) render3dbox(d->o, d->eyeheight, d->aboveeye, d->radius);
        else if((anim&ANIM_INDEX)!=ANIM_GUNSHOOT && (anim&ANIM_INDEX)!=ANIM_GUNIDLE)
        {
            vec center, radius;
            if(showboundingbox==1) m->collisionbox(0, center, radius);
            else m->boundbox(0, center, radius);
            rotatebb(center, radius, int(yaw));
            center.add(vec(x, y, z));
            render3dbox(center, radius.z, radius.z, radius.x, radius.y);
        };
    };

    if(d) lightreaching(d->o, color, dir);
    m->setskin(tex);  
    glColor3fv(color.v);
    m->setshader();
    if(renderpath!=R_FIXEDFUNCTION)
    {
        vec rdir(dir);
        rdir.rotate_around_z((-yaw-180.0f)*RAD);
        rdir.rotate_around_y(-pitch*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 0, rdir.x, rdir.y, rdir.z, 0);

        vec camerapos = vec(player->o).sub(vec(x, y, z));
        camerapos.rotate_around_z((-yaw-180.0f)*RAD);
        camerapos.rotate_around_y(-pitch*RAD);
        glProgramEnvParameter4f_(GL_VERTEX_PROGRAM_ARB, 1, camerapos.x, camerapos.y, camerapos.z, 1);

        if(refracting) setfogplane(1, refracting - z);
    };

    model *vwep = NULL;
    if(vwepmdl)
    {
        vwep = loadmodel(vwepmdl);
        if(vwep->type()!=m->type()) vwep = NULL;
    };

    if(!m->cullface) glDisable(GL_CULL_FACE);
    m->render(anim, varseed, speed, basetime, x, y, z, yaw, pitch, d, vwep);
    if(!m->cullface) glEnable(GL_CULL_FACE);
};

void abovemodel(vec &o, const char *mdl)
{
    model *m = loadmodel(mdl);
    if(!m) return;
    o.z += m->above(0/*frame*/);
};

int findanim(const char *name)
{
    const char *names[] = { "dying", "dead", "pain", "idle", "idle attack", "run", "run attack", "edit", "lag", "jump", "jump attack", "gun shoot", "gun idle", "mapmodel", "trigger" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
};

void loadskin(const char *dir, const char *altdir, Texture *&skin, Texture *&masks, model *m) // model skin sharing
{
#define ifnoload(tex, path) if((tex = textureload(path, false, true, false))==crosshair)
#define tryload(tex, path, name, success, fail) \
    s_sprintfd(path)("packages/models/%s/%s.jpg", dir, name); \
    ifnoload(tex, path) \
    { \
        strcpy(path+strlen(path)-3, "png"); \
        ifnoload(tex, path) \
        { \
            s_sprintf(path)("packages/models/%s/%s.jpg", altdir, name); \
            ifnoload(tex, path) \
            { \
                strcpy(path+strlen(path)-3, "png"); \
                ifnoload(tex, path) fail; \
            } else success; \
        } else success; \
    } else success;
        
    if(renderpath==R_FIXEDFUNCTION) masks = crosshair;
    else
    {
        tryload(masks, maskspath, "masks", m->masked = true, {});
    };
    tryload(skin, skinpath, "skin", {}, return);
};

// convenient function that covers the usual anims for players/monsters/npcs

void renderclient(dynent *d, const char *mdlname, const char *vwepname, bool forceattack, int lastaction, int lastpain)
{
    int anim = ANIM_IDLE|ANIM_LOOP;
    float speed = 100.0f;
    float mz = d->o.z-d->eyeheight;     
    int basetime = -((int)(size_t)d&0xFFF);
    bool attack = (forceattack || (d->type!=ENT_AI && lastmillis-lastaction<200));
    if(d->state==CS_DEAD)
    {
        anim = ANIM_DYING;
        int r = 6;//FIXME, 3rd anim & hellpig take longer
        basetime = lastaction;
        int t = lastmillis-lastaction;
        if(t<0 || t>20000) return;
        if(t>(r-1)*100) { anim = ANIM_DEAD|ANIM_LOOP; if(t>(r+10)*100) { t -= (r+10)*100; mz -= t*t/10000000000.0f*t/16.0f; }; };
        if(mz<-1000) return;
    }
    else if(d->state==CS_EDITING || d->state==CS_SPECTATOR) { anim = ANIM_EDIT|ANIM_LOOP; }
    else if(d->state==CS_LAGGED)                            { anim = ANIM_LAG|ANIM_LOOP; }
    else if(lastmillis-lastpain<300)                        { anim = ANIM_PAIN|ANIM_LOOP; }
    else
    {
        if(d->timeinair>100)            { anim = attack ? ANIM_JUMP_ATTACK : ANIM_JUMP|ANIM_END; }
        else if(!d->move && !d->strafe) { anim = attack ? ANIM_IDLE_ATTACK : ANIM_IDLE|ANIM_LOOP; }
        else                            { anim = attack ? ANIM_RUN_ATTACK : ANIM_RUN|ANIM_LOOP; speed = 5500/d->maxspeed; };
        if(attack) basetime = lastaction;
    };
    vec color, dir;
    rendermodel(color, dir, mdlname,  anim, (int)(size_t)d, 0, d->o.x, d->o.y, mz, d->yaw+90, d->pitch/4, speed, basetime, d, (MDL_CULL_VFC | MDL_CULL_OCCLUDED) | (d->type==ENT_PLAYER ? 0 : MDL_CULL_DIST), vwepname);
};

void setbbfrommodel(dynent *d, char *mdl)
{
    model *m = loadmodel(mdl); 
    if(!m) return;
    vec center, radius;
    m->collisionbox(0, center, radius);
    d->radius    = max(radius.x+fabs(center.x), radius.y+fabs(center.y));
    d->eyeheight = (center.z-radius.z) + radius.z*2*m->eyeheight;
    d->aboveeye  = radius.z*2*(1.0f-m->eyeheight);
};

void vectoyawpitch(const vec &v, float &yaw, float &pitch)
{
    yaw = -(float)atan2(v.x, v.y)/RAD + 180;
    pitch = asin(v.z/v.magnitude())/RAD;
};


