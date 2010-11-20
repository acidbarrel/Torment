// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "pch.h"
#include "engine.h"

static vec menupos;
static int menustart = 0;
static int menutab = 1;
static g3d_gui *cgui = NULL;

static hashtable<char *, char *> guis;
static vector<char *> guistack;
static vector<char *> executelater;
static bool clearlater = false;

VARP(menudistance, 16, 40, 256);

vec menuinfrontofplayer() 
{ 
    vec dir;
    vecfromyawpitch(camera1->yaw, 0, 1, 0, dir, false);
    dir.mul(menudistance).add(camera1->o);
    dir.z -= player->eyeheight-1;
    return dir;
};

int cleargui(int n = 0)
{
    int clear = guistack.length();
    if(n>0) clear = min(clear, n);
    loopi(clear) delete[] guistack.pop();
    if(!guistack.empty()) showgui(guistack.last());
    return clear;
};

void cleargui_(int *n)
{
    intret(cleargui(*n));
};

#define GUI_TITLE_COLOR  0xFFDD88
#define GUI_BUTTON_COLOR 0xFFFFFF
#define GUI_TEXT_COLOR   0xDDFFDD

//@DOC name and icon are optional
void guibutton(char *name, char *action, char *icon)
{
    if(!cgui) return;
    int ret = cgui->button(name, GUI_BUTTON_COLOR, *icon ? icon : (strstr(action, "showgui") ? "menu" : "action"));
    if(ret&G3D_UP) 
    {
        executelater.add(newstring(*action ? action : name));
        clearlater = true;
    }
    else if(ret&G3D_ROLLOVER)
    {
        alias("guirollovername", name);
        alias("guirolloveraction", action);
    };
};

void guiimage(char *path, char *action, float *scale, int *overlaid)
{
    if(cgui && cgui->image(path, *scale, *overlaid!=0)&G3D_UP && *action)
    {
        executelater.add(newstring(action));
        clearlater = true;
    };
};

void guitext(char *name)
{
    if(cgui) cgui->text(name, GUI_TEXT_COLOR, "info");
};

void guititle(char *name)
{
    if(cgui) cgui->title(name, GUI_TITLE_COLOR);
};

void guitab(char *name)
{
    if(cgui) cgui->tab(name, GUI_TITLE_COLOR);
};

void guibar()
{
    if(cgui) cgui->separator();
};

static void updateval(char *var, int val, char *onchange)
{
    ident *id = getident(var);
    string assign;
    if(!id) return;
    else if(id->_type==ID_VAR) s_sprintf(assign)("%s %d", var, val);
    else if(id->_type==ID_ALIAS) s_sprintf(assign)("%s = %d", var, val);
    else return;
    executelater.add(newstring(assign));
    if(onchange[0]) executelater.add(newstring(onchange)); 
};

static int getval(char *var)
{
    ident *id = getident(var);
    if(!id) return 0;
    else if(id->_type==ID_VAR) return *id->_storage;
    else if(id->_type==ID_ALIAS) return atoi(id->_action);
    else return 0;
};

void guislider(char *var, int *min, int *max, char *onchange)
{
	if(!cgui) return;
    int oldval = getval(var), val = oldval, vmin = *max ? *min : getvarmin(var), vmax = *max ? *max : getvarmax(var);
    cgui->slider(val, vmin, vmax, GUI_TITLE_COLOR);
    if(val != oldval) updateval(var, val, onchange);
};

void guicheckbox(char *name, char *var, int *on, int *off, char *onchange)
{
    bool enabled = getval(var)!=*off;
    if(cgui && cgui->button(name, GUI_BUTTON_COLOR, enabled ? "checkbox_on" : "checkbox_off")&G3D_UP)
    {
        updateval(var, enabled ? *off : (*on || *off ? *on : 1), onchange);
    };
};

void guiradio(char *name, char *var, int *n, char *onchange)
{
    bool enabled = getval(var)==*n;
    if(cgui && cgui->button(name, GUI_BUTTON_COLOR, enabled ? "radio_on" : "radio_off")&G3D_UP)
    {
        if(!enabled) updateval(var, *n, onchange);
    };
};

void guilist(char *contents)
{
    if(!cgui) return;
    cgui->pushlist();
    execute(contents);
    cgui->poplist();
};

void newgui(char *name, char *contents) 
{ 
    if(guis.access(name))
    {
        delete[] guis[name];
        guis[name] = newstring(contents);
    }
    else guis[newstring(name)] = newstring(contents); 
};

void showgui(char *name)
{
    int pos = guistack.find(name);
    if(pos<0) 
    {   
        if(!guis.access(name)) return;
        if(guistack.empty()) menupos = menuinfrontofplayer();
        guistack.add(newstring(name));
    }
    else
    {
        pos = guistack.length()-pos-1;
        loopi(pos) delete[] guistack.pop();
    };
    menutab = 1;
    menustart = lastmillis;    
};

void guiservers()
{
    extern const char *showservers(g3d_gui *cgui);
    if(cgui) 
    {
        const char *name = showservers(cgui); 
        if(name)
        {
            s_sprintfd(connect)("connect %s", name);
            executelater.add(newstring(connect));
            clearlater = true;
        };
    };
};

COMMAND(newgui, "ss");
COMMAND(guibutton, "sss");
COMMAND(guitext, "s");
COMMAND(guiservers, "s");
COMMANDN(cleargui, cleargui_, "i");
COMMAND(showgui, "s");

COMMAND(guilist, "s");
COMMAND(guititle, "s");
COMMAND(guibar,"");
COMMAND(guiimage,"ssfi");
COMMAND(guislider,"siis");
COMMAND(guiradio,"ssis");
COMMAND(guicheckbox, "ssiis");
COMMAND(guitab, "s");

static struct mainmenucallback : g3d_callback
{
    void gui(g3d_gui &g, bool firstpass)
    {
        if(guistack.empty()) return;
        char *name = guistack.last();
        char **contents = guis.access(name);
        if(!contents) return;
		cgui = &g;
        cgui->start(menustart, 0.03f, &menutab);
		guitab(name);		
		execute(*contents);
        cgui->end();
		cgui = NULL;
    };
} mmcb;

void menuprocess()
{
    int level = guistack.length();
    loopv(executelater) execute(executelater[i]);
    executelater.deletecontentsa();
    if(clearlater)
    {
        if(level==guistack.length()) guistack.deletecontentsa();
        clearlater = false;
    };
};

void g3d_mainmenu()
{
    if(!guistack.empty()) 
    {   
        if(camera1->o.dist(menupos) > menudistance*3) cleargui();
        else g3d_addgui(&mmcb, menupos);
    };
};

