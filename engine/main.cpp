// main.cpp: initialisation & main loop

#include "pch.h"
#include "engine.h"

void quit()                     // normal exit
{
    writeservercfg();
    disconnect(true);
    if(strcmp(cl->gameident(), "fps")==0) writecfg();       // TEMP HACK: make other games not overwrite cfg
    cleangl();
    cleanupserver();
    SDL_ShowCursor(1);
    freeocta(worldroot);
    extern void clear_command(); clear_command();
    extern void clear_console(); clear_console();
    extern void clear_mdls();    clear_mdls();
    extern void clear_sound();   clear_sound();
    SDL_Quit();
    exit(EXIT_SUCCESS);
};

void fatal(char *s, char *o)    // failure exit
{
    s_sprintfd(msg)("%s%s\n", s, o);
    printf(msg);
    #ifdef WIN32
        MessageBox(NULL, msg, "sauerbraten fatal error", MB_OK|MB_SYSTEMMODAL);
    #endif
    exit(EXIT_FAILURE);
};

SDL_Surface *screen = NULL;

int curtime;
int lastmillis = 0;

dynent *player = NULL;

int scr_w = 640, scr_h = 480;

void screenshot()
{
    SDL_Surface *image;
    SDL_Surface *temp;
    int idx;
    if((image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)))
    {
        if((temp = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)))
        {
            glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
            for (idx = 0; idx<scr_h; idx++)
            {
                char *dest = (char *)temp->pixels+temp->pitch*idx;
                memcpy(dest, (char *)image->pixels+image->pitch*(scr_h-1-idx), 3*scr_w);
                endianswap(dest, 3, scr_w);
            };
            s_sprintfd(buf)("screenshot_%d.bmp", lastmillis);
            SDL_SaveBMP(temp, buf);
            SDL_FreeSurface(temp);
        };
        SDL_FreeSurface(image);
    };
};

COMMAND(screenshot, "");
COMMAND(quit, "");

void computescreen(const char *text)
{
    int w = scr_w, h = scr_h;
    gettextres(w, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.15f, 0.15f, 0.15f, 1);
    loopi(2)
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, w*3, h*3, 0, -1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_text(text, 70, 2*FONTH + FONTH/2);
        settexture("data/doom.jpg");
        glLoadIdentity();
        glOrtho(0, w, h, 0, -1, 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        int x = (w-1024), y = (h-768);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(x,     y);
        glTexCoord2f(1, 0); glVertex2i(x+1024, y);
        glTexCoord2f(1, 1); glVertex2i(x+1024, y+768);
        glTexCoord2f(0, 1); glVertex2i(x,     y+768);
        glEnd();
        SDL_GL_SwapBuffers();
    };
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
};

void bar(float bar, int w, int o, float r, float g, float b)
{
    int side = 0;
    glColor3f(r, g, b);
    glVertex2f(side,                  0);
    glVertex2f(bar*(w*3-2*side)+side, 0);
    glVertex2f(bar*(w*3-2*side)+side, 0);
    glVertex2f(side,                  0);
};

void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2, const char *text2)   // also used during loading
{
    if(!inbetweenframes) return;

    clientkeepalive();      // make sure our connection doesn't time out while loading maps etc.

    int w = scr_w, h = scr_h;
    gettextres(w, h);

    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, w*3, h*3, 0, -1, 1);
    notextureshader->set();

    glBegin(GL_QUADS);

    if(text1)
    {
        bar(1,    w, 4, 0, 0,    0.8f);
        bar(bar1, w, 4, 0, 0.5f, 1);
    };

    if(bar2>0)
    {
        bar(1,    w, 6, 0.5f,  0, 0);
        bar(bar2, w, 6, 0.75f, 0, 0);
    };

    glEnd();

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    defaultshader->set();

    if(text1) draw_text(text1, 50, 50);
    if(bar2>0) draw_text(text2, 1200, 6*FONTH + FONTH/2);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    SDL_GL_SwapBuffers();
};

void setfullscreen(bool enable)
{
    if(enable == !(screen->flags&SDL_FULLSCREEN))
    {
#if defined(WIN32) || defined(__APPLE__)
        conoutf("\"fullscreen\" variable not supported on this platform. Use the -t command-line option.");
        extern int fullscreen;
        fullscreen = !enable;
#else
        SDL_WM_ToggleFullScreen(screen);
        SDL_WM_GrabInput((screen->flags&SDL_FULLSCREEN) ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
    };
};

void screenres(int *w, int *h, int *bpp = 0)
{
#if defined(WIN32) || defined(__APPLE__)
    conoutf("\"screenres\" command not supported on this platform. Use the -w and -h command-line options.");
#else
    SDL_Surface *surf = SDL_SetVideoMode(*w, *h, bpp ? *bpp : 0, SDL_OPENGL|SDL_RESIZABLE|(screen->flags&SDL_FULLSCREEN));
    if(!surf) return;
    screen = surf;
    scr_w = screen->w;
    scr_h = screen->h;
    glViewport(0, 0, scr_w, scr_h);
#endif
};

VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));

COMMAND(screenres, "iii");

VARFP(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", SDL_GetError());
    };
});

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
                             SDL_DEFAULT_REPEAT_INTERVAL);
};

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);

struct sleepcmd
{
    int millis;
    char *command;
};
vector<sleepcmd> sleepcmds;

void addsleep(int *msec, char *cmd)
{
    sleepcmd &s = sleepcmds.add(); 
    s.millis = *msec+lastmillis; 
    s.command = newstring(cmd); 
};

COMMANDN(sleep, addsleep, "is");

void checksleep(int millis)
{
    loopv(sleepcmds)
    {
        sleepcmd &s = sleepcmds[i];
        if(s.millis && millis>s.millis)
        {
            char *cmd = s.command; // execute might create more sleep commands
            execute(cmd);
            delete[] cmd; 
            sleepcmds.remove(i--); 
        };
    };
};

VARF(paused, 0, 0, 1, if(multiplayer()) paused = 0);

void estartmap(const char *name)
{
    ///if(!editmode) toggleedit();
    gamespeed = 100;
    paused = 0;
    loopv(sleepcmds) delete[] sleepcmds[i].command;
    sleepcmds.setsize(0);
    cancelsel();
    pruneundos();
    setvar("wireframe", 0);
    cl->startmap(name);
};

VAR(maxfps, 5, 200, 500);

void limitfps(int &millis, int curmillis)
{
    static int fpserror = 0;
    int delay = 1000/maxfps - (millis-curmillis);
    if(delay < 0) fpserror = 0;
    else
    {
        fpserror += 1000%maxfps;
        if(fpserror >= maxfps)
        {
            ++delay;
            fpserror -= maxfps;
        };
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        };
    };
};

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    string out, t;
    s_sprintf(out)("Sauerbraten Win32 Exception: 0x%x [0x%x]\n\n", er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    STACKFRAME sf = {{context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    while(::StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        struct { IMAGEHLP_SYMBOL sym; string n; } si = { { sizeof( IMAGEHLP_SYMBOL ), 0, 0, 0, sizeof(string) } };
        IMAGEHLP_LINE li = { sizeof( IMAGEHLP_LINE ) };
        DWORD off;
        if(SymGetSymFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &si.sym) && SymGetLineFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &li))
        {
            char *del = strrchr(li.FileName, '\\');
            s_sprintf(t)("%s - %s [%d]\n", si.sym.Name, del ? del + 1 : li.FileName, li.LineNumber);
            s_strcat(out, t);
        };
    };
    fatal(out);
};
#endif

bool inbetweenframes = false;

int main(int argc, char **argv)
{
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #ifndef _DEBUG
    #ifndef __GNUC__
    __try {
    #endif
    #endif
    #endif

    bool dedicated = false;
    int fs = SDL_FULLSCREEN, par = 0, depth = 0, bpp = 0, fsaa = 0;
    char *load = NULL;
    
    #define log(s) puts("init: " s)
    log("sdl");

    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'd': dedicated = true; break;
            case 'w': scr_w = atoi(&argv[i][2]); scr_h = scr_w*3/4; break;
            case 'h': scr_h = atoi(&argv[i][2]); break;
            case 'z': depth = atoi(&argv[i][2]); break;
            case 'b': bpp = atoi(&argv[i][2]); break;
            case 'a': fsaa = atoi(&argv[i][2]); break;
            case 't': fs = 0; break;
            case 'f': 
            {
                extern int shaderprecision; 
                shaderprecision = atoi(&argv[i][2]); 
                shaderprecision = min(max(shaderprecision, 0), 3);
                break;
            };
            case 'l': 
            {
                char pkgdir[] = "packages/"; 
                load = strstr(path(&argv[i][2]), path(pkgdir)); 
                if(load) load += sizeof(pkgdir)-1; 
                else load = &argv[i][2]; 
                break;
            };
            default:  if(!serveroption(argv[i])) conoutf("unknown commandline option");
        }
        else conoutf("unknown commandline argument");
    };

    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    fs = 0;
    SetEnvironmentVariable("SDL_DEBUG", "1");
    #endif

    //#ifdef WIN32
    //SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    //#endif

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO|par)<0) fatal("Unable to initialize SDL: ", SDL_GetError());

    log("enet");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    initserver(dedicated);  // never returns if dedicated

    log("video: mode");
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if(depth) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth); 
    if(fsaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, fsaa);
    };
    int resize = SDL_RESIZABLE;
    #if defined(WIN32) || defined(__APPLE__)
    resize = 0;
    #endif
    screen = SDL_SetVideoMode(scr_w, scr_h, bpp, SDL_OPENGL|resize|fs);
    if(screen==NULL) fatal("Unable to create OpenGL screen: ", SDL_GetError());
    scr_w = screen->w;
    scr_h = screen->h;

    fullscreen = fs!=0;

    log("video: misc");
    SDL_WM_SetCaption("sauerbraten engine", NULL);
    #ifndef WIN32
    if(fs)
    #endif
    SDL_WM_GrabInput(SDL_GRAB_ON);
    keyrepeat(false);
    SDL_ShowCursor(0);

    log("console");
    persistidents = false;
    if(!execfile("data/stdlib.cfg")) fatal("cannot find data files (you are running from the wrong folder, try .bat file in the main folder)");   // this is the first file we load.

    log("gl");
    gl_init(scr_w, scr_h, bpp, depth, fsaa);
    crosshair = textureload("data/crosshair.png", true, false);
    if(!crosshair) fatal("could not find core textures");
    computescreen("initializing...");
    inbetweenframes = true;
    particleinit();

    log("world");
    camera1 = player = cl->iterdynents(0);
    emptymap(0, true);

    log("sound");
    initsound();

    log("cfg");
    exec("data/keymap.cfg");
    exec("data/stdedit.cfg");
    exec("data/menus.cfg");
    exec("data/sounds.cfg");
    exec("data/brush.cfg");
    execfile("mybrushes.cfg");
    execfile("servers.cfg");
    
    persistidents = true;
    
    if(!execfile("config.cfg")) exec("data/defaults.cfg");
    execfile("autoexec.cfg");

    persistidents = false;

    string gamecfgname;
    s_strcpy(gamecfgname, "data/game_");
    s_strcat(gamecfgname, cl->gameident());
    s_strcat(gamecfgname, ".cfg");
    exec(gamecfgname);

    persistidents = true;

    log("localconnect");
    localconnect();
    cc->gameconnect(false);
    cc->changemap(load ? load : "forward_base");

    log("mainloop");
    int ignore = 5, grabmouse = 0;
    for(;;)
    {
        static int frames = 0;
        static float fps = 10.0;
        static int curmillis = 0;
        int millis = SDL_GetTicks();
        limitfps(millis, curmillis);
        int elapsed = millis-curmillis;
        curtime = elapsed*gamespeed/100;
        if(curtime>200) curtime = 200;
        else if(curtime<1) curtime = 1;
        if(paused) curtime = 0;
        
        if(lastmillis) cl->updateworld(worldpos, curtime, lastmillis);
       
        checksleep(millis);
        
        menuprocess();

        lastmillis += curtime;
        curmillis = millis;

        checksleep(lastmillis);

        serverslice(time(NULL), 0);

        frames++;
        fps = (1000.0f/elapsed+fps*10)/11;
        //if(curtime>14) printf("%d: %d\n", millis, curtime);

        extern void updatevol(); updatevol();

        inbetweenframes = false;
        SDL_GL_SwapBuffers();
        if(frames>2) gl_drawframe(scr_w, scr_h, fps);
        //SDL_Delay(10);
        inbetweenframes = true;

        SDL_Event event;
        int lasttype = 0, lastbut = 0;
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                case SDL_QUIT:
                    quit();
                    break;

                #if !defined(WIN32) && !defined(__APPLE__)
                case SDL_VIDEORESIZE:
                    screenres(&event.resize.w, &event.resize.h);
                    break;
                #endif

                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode);
                    break;

                case SDL_ACTIVEEVENT:
                    if(event.active.state & SDL_APPINPUTFOCUS)
                        grabmouse = event.active.gain;
                    else
                    if(event.active.gain)
                        grabmouse = 1;
                    break;

                case SDL_MOUSEMOTION:
                    if(ignore) { ignore--; break; };
                    if(!(screen->flags&SDL_FULLSCREEN) && grabmouse)
                    {   
                        #ifdef __APPLE__
                        if(event.motion.y == 0) break;  //let mac users drag windows via the title bar
                        #endif
                        if(event.motion.x == scr_w / 2 && event.motion.y == scr_h / 2) break;
                        SDL_WarpMouse(scr_w / 2, scr_h / 2);
                    };
                    #ifndef WIN32
                    if((screen->flags&SDL_FULLSCREEN) || grabmouse)
                    #endif
                    mousemove(event.motion.xrel, event.motion.yrel);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                    keypress(-event.button.button, event.button.state!=0, 0);
                    lasttype = event.type;
                    lastbut = event.button.button;
                    break;
            };
        };
    };
    
    ASSERT(0);   
    return EXIT_FAILURE;

    #if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; };
    #endif
};
