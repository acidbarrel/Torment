// rendertext.cpp: based on Don's gl_text.cpp

#include "pch.h"
#include "engine.h"

short char_coords[94][3] = 
{
    { 7  , 0  , 12 },  //!
    { 26 , 0  , 19 },  //"
    { 52 , 0  , 35 },  //#
    { 94 , 0  , 27 },  //$
    { 128, 0  , 50 },  //%
    { 185, 0  , 38 },  //&
    { 230, 0  , 9  },  //'
    { 246, 0  , 19 },  //(
    { 272, 0  , 18 },  //)
    { 297, 0  , 27 },  //*
    { 331, 0  , 35 },  //+
    { 373, 0  , 13 },  //,
    { 393, 0  , 17 },  //-
    { 417, 0  , 11 },  //.
    { 435, 0  , 20 },  ///
    { 462, 0  , 27 },  //0
    { 0  , 71 , 25 },  //1
    { 32 , 71 , 27 },  //2
    { 173, 71 , 26 },  //3
    { 206, 71 , 28 },  //4
    { 241, 71 , 27 },  //5
    { 275, 71 , 28 },  //6
    { 310, 71 , 28 },  //7
    { 345, 71 , 27 },  //8
    { 379, 71 , 27 },  //9
    { 413, 71 , 13 },  //:
    { 433, 71 , 15 },  //;
    { 455, 71 , 33 },  //<
    { 0  , 142, 34 },  //=
    { 148, 142, 34 },  //>
    { 189, 142, 24 },  //?
    { 220, 142, 46 },  //@
    { 273, 142, 33 },  //A
    { 313, 142, 30 },  //B
    { 350, 142, 31 },  //C
    { 388, 142, 35 },  //D
    { 430, 142, 28 },  //E
    { 465, 142, 27 },  //F
    { 0  , 213, 33 },  //G
    { 40 , 213, 32 },  //H
    { 79 , 213, 18 },  //I
    { 104, 213, 19 },  //J
    { 130, 213, 32 },  //K
    { 169, 213, 27 },  //L
    { 310, 213, 37 },  //M
    { 354, 213, 32 },  //N
    { 393, 213, 36 },  //O
    { 436, 213, 29 },  //P
    { 472, 213, 36 },  //Q
    { 0  , 284, 34 },  //R
    { 41 , 284, 29 },  //S
    { 77 , 284, 32 },  //T
    { 116, 284, 32 },  //U
    { 155, 284, 33 },  //V
    { 195, 284, 48 },  //W
    { 250, 284, 31 },  //X
    { 288, 284, 32 },  //Y
    { 327, 284, 29 },  //Z
    { 363, 284, 18 },  //[
    { 388, 284, 21 },  //"\"
    { 416, 284, 16 },  //]
    { 439, 284, 36 },  //^
    { 0  , 355, 31 },  //_
    { 203, 213, 18 },  //`      //TODO
	//LOWER CASE// EDIT BY AMIGA-PIRATE
	{ 273, 142, 33 },  //A
    { 313, 142, 30 },  //B
    { 350, 142, 31 },  //C
    { 388, 142, 35 },  //D
    { 430, 142, 28 },  //E
    { 465, 142, 27 },  //F
    { 0  , 213, 33 },  //G
    { 40 , 213, 32 },  //H
    { 79 , 213, 18 },  //I
    { 104, 213, 19 },  //J
    { 130, 213, 32 },  //K
    { 169, 213, 27 },  //L
    { 310, 213, 37 },  //M
    { 354, 213, 32 },  //N
    { 393, 213, 36 },  //O
    { 436, 213, 29 },  //P
    { 472, 213, 36 },  //Q
    { 0  , 284, 34 },  //R
    { 41 , 284, 29 },  //S
    { 77 , 284, 32 },  //T
    { 116, 284, 32 },  //U
    { 155, 284, 33 },  //V
    { 195, 284, 48 },  //W
    { 250, 284, 31 },  //X
    { 288, 284, 32 },  //Y
    { 327, 284, 29 },  //Z
   // { 38 , 355, 25 },  //a
   // { 70 , 355, 28 },  //b
   // { 105, 355, 24 },  //c
   // { 136, 355, 26 },  //d
   // { 169, 355, 27 },  //e
   // { 203, 355, 19 },  //f
   // { 229, 355, 26 },  //g
   // { 262, 355, 27 },  //h
   // { 296, 355, 9  },  //i
   // { 312, 355, 15 },  //j
   // { 334, 355, 28 },  //k
   // { 369, 355, 9  },  //l
   // { 385, 355, 42 },  //m
   // { 434, 355, 27 },  //n
   // { 468, 355, 28 },  //o
   // { 0  , 426, 28 },  //p
   // { 35 , 426, 26 },  //q
   // { 68 , 426, 19 },  //r
   // { 94 , 426, 23 },  //s
   // { 124, 426, 18 },  //t
   // { 149, 426, 26 },  //u
   // { 182, 426, 27 },  //v
   // { 216, 426, 39 },  //w
   // { 262, 426, 26 },  //x
   // { 295, 426, 27 },  //y
   // { 329, 426, 23 },  //z
    { 359, 426, 23 },  //{
    { 496, 426, 13 },  //|
    { 389, 426, 23 },  //} // coords
    { 419, 426, 34 },  //~  // coords
};                     

void gettextres(int &w, int &h)
{
    if(w < MINRESW || h < MINRESH)
    {
        if(MINRESW > w*MINRESH/h)
        {
            h = h*MINRESW/w;
            w = MINRESW;
        }
        else
        {
            w = w*MINRESH/h;
            h = MINRESH;
        };
    };
};

#define PIXELTAB (FONTH*4)

int char_width(int c, int x)
{
    if(c=='\t') x = (x+PIXELTAB)/PIXELTAB*PIXELTAB;
    else if(c==' ') x += FONTH/2;
    else if(c>=33 && c<=126)
    {
        c -= 33;
        int in_width = char_coords[c][2];
        x += in_width + 1;
    };
    return x;
};

int text_width(const char *str, int limit)
{
    int x = 0;
    for(int i = 0; str[i] && (limit<0 ||i<limit); i++) 
    {
        if(str[i]=='\f')
        {
            i++;
            continue;
        };
        x = char_width(str[i], x);
    };
    return x;
}

int text_visible(const char *str, int max)
{
    int i = 0, x = 0;
    while(str[i])
    {
        if(str[i]=='\f')
        {
            i += 2;
            continue;
        };
        x = char_width(str[i], x);
        if(x > max) return i;
        ++i;
    };
    return i;
};
 
void draw_textf(const char *fstr, int left, int top, ...)
{
    s_sprintfdlv(str, top, fstr);
    draw_text(str, left, top);
};

void draw_text(const char *str, int left, int top, int r, int g, int b, int a)
{
    static Texture *charstex = NULL;
    if(!charstex) charstex = textureload("data/newerchars.png");

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	//GL_ONE_MINUS_SRC_ALPHA
    glBindTexture(GL_TEXTURE_2D, charstex->gl);
    glColor4ub(r, g, b, a);
    //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    int x = left;
    int y = top;

    int i;
    float in_left, in_top, in_right, in_bottom;
    int in_width, in_height;

    glBegin(GL_QUADS);
    for (i = 0; str[i] != 0; i++)
    {
        int c = str[i];
        if(c=='\t') { x = (x-left+PIXELTAB)/PIXELTAB*PIXELTAB+left; continue; }; 
        if(c=='\f') switch(str[i+1])
        {
            case '0': glColor3ub(64,255,128); i++; continue;    // green: player talk
            case '1': glColor3ub(96,160,255); i++; continue;    // blue: "echo" command
            case '2': glColor3ub(255,192,64); i++; continue;    // yellow: gameplay messages 
            case '3': glColor3ub(255,64,64);  i++; continue;    // red: important errors
            default: continue;                                  // white: everything else
        };
        if(c==' ') { x += FONTH/2; continue; };
        c -= 33;
        if(c<0 || c>=95) continue;
        
        in_width   = char_coords[c][2];
        in_height  = 64;
        in_left    = ((float) char_coords[c][0]+2-2)   / 512.0f;
        in_top     = ((float) char_coords[c][1]+2-3) / 512.0f;
        in_right   = ((float) char_coords[c][0]+2+3+in_width)   / 512.0f;
        in_bottom  = ((float) char_coords[c][1]+2+4+in_height) / 512.0f;


        glTexCoord2f(in_left,  in_top   ); glVertex2i(x,            y);
        glTexCoord2f(in_right, in_top   ); glVertex2i(x + in_width, y);
        glTexCoord2f(in_right, in_bottom); glVertex2i(x + in_width, y + in_height);
        glTexCoord2f(in_left,  in_bottom); glVertex2i(x,            y + in_height);
        
        xtraverts += 4;
        x += in_width  + 1;
    };
    glEnd();
};

Texture *sky[6] = { 0, 0, 0, 0, 0, 0 };

void loadsky(char *basename)
{
    static string lastsky = "";
    if(strcmp(lastsky, basename)==0) return;
    static char *side[] = { "ft", "bk", "lf", "rt", "dn", "up" };
    loopi(6)
    {
        s_sprintfd(name)("packages/%s_%s.jpg", basename, side[i]);
        if((sky[i] = textureload(name, true))==crosshair) conoutf("could not load sky textures");
        // FIXME? now doesn't overwrite old sky any more which uses more memory, but gives faster loadtimes...
    };
    s_strcpy(lastsky, basename);
};

COMMAND(loadsky, "s");

void draw_envbox_face(float s0, float t0, int x0, int y0, int z0,
                      float s1, float t1, int x1, int y1, int z1,
                      float s2, float t2, int x2, int y2, int z2,
                      float s3, float t3, int x3, int y3, int z3,
                      GLuint texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(s3, t3); glVertex3i(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3i(x2, y2, z2);
    glTexCoord2f(s1, t1); glVertex3i(x1, y1, z1);
    glTexCoord2f(s0, t0); glVertex3i(x0, y0, z0);
    glEnd();
    xtraverts += 4;
};

void draw_envbox(int w, float zclip)
{
    if(!sky[0]) fatal("no skybox");

    float vclip = 1-zclip;
    int z = int(ceil(2*w*(vclip-0.5f)));

    glDepthMask(GL_FALSE);

    draw_envbox_face(1.0f, vclip, -w, -w,  z,
                     0.0f, vclip,  w, -w,  z,
                     0.0f, 0.0f,  w, -w, -w,
                     1.0f, 0.0f, -w, -w, -w, sky[0]->gl);

    draw_envbox_face(1.0f, vclip, +w,  w,  z,
                     0.0f, vclip, -w,  w,  z,
                     0.0f, 0.0f, -w,  w, -w,
                     1.0f, 0.0f, +w,  w, -w, sky[1]->gl);

    draw_envbox_face(0.0f, 0.0f, -w, -w, -w,
                     1.0f, 0.0f, -w,  w, -w,
                     1.0f, vclip, -w,  w,  z,
                     0.0f, vclip, -w, -w,  z, sky[2]->gl);

    draw_envbox_face(1.0f, vclip, +w, -w,  z,
                     0.0f, vclip, +w,  w,  z,
                     0.0f, 0.0f, +w,  w, -w,
                     1.0f, 0.0f, +w, -w, -w, sky[3]->gl);

    if(!zclip)
        draw_envbox_face(0.0f, 1.0f, -w,  w,  w,
                         0.0f, 0.0f, +w,  w,  w,
                         1.0f, 0.0f, +w, -w,  w,
                         1.0f, 1.0f, -w, -w,  w, sky[4]->gl);

    draw_envbox_face(0.0f, 1.0f, +w,  w, -w,
                     0.0f, 0.0f, -w,  w, -w,
                     1.0f, 0.0f, -w, -w, -w,
                     1.0f, 1.0f, +w, -w, -w, sky[5]->gl);

    glDepthMask(GL_TRUE);
};
