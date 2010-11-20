// implementation of generic tools

#include "pch.h"
#include "tools.h"

///////////////////////// misc tools ///////////////////////

char *path(char *s)
{
    for(char *t = s; (t = strpbrk(t, "/\\")); *t++ = PATHDIV);
    return s;
};

char *parentdir(char *directory)
{
    char *p = strrchr(directory, '/');
    if(!p) p = directory;
    size_t len = p-directory+1;
    char *parent = new char[len];
    s_strncpy(parent, directory, len);
    return parent;
};

char *loadfile(char *fn, int *size)
{
    FILE *f = fopen(fn, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    if(len<=0) { fclose(f); return NULL; };
    fseek(f, 0, SEEK_SET);
    char *buf = new char[len+1];
    if(!buf) { fclose(f); return NULL; };
    buf[len] = 0;
    size_t rlen = fread(buf, 1, len, f);
    fclose(f);
    if(size_t(len)!=rlen)
    {
        delete[] buf;
        return NULL;
    };
    if(size!=NULL) *size = len;
    return buf;
};

void endianswap(void *memory, int stride, int length)   // little endian as storage format
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    loop(w, length) loop(i, stride/2)
    {
        uchar *p = (uchar *)memory+w*stride;
        uchar t = p[i];
        p[i] = p[stride-i-1];
        p[stride-i-1] = t;
    };
#endif
};


////////////////////////// rnd numbers ////////////////////////////////////////

#define N              (624)             
#define M              (397)                
#define K              (0x9908B0DFU)       
#define hiBit(u)       ((u) & 0x80000000U)  
#define loBit(u)       ((u) & 0x00000001U)  
#define loBits(u)      ((u) & 0x7FFFFFFFU)  
#define mixBits(u, v)  (hiBit(u)|loBits(v)) 

static uint state[N+1];     
static uint *next;          
static int left = -1;     

void seedMT(uint seed)
{
    register uint x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
    register int j;
    for(left=0, *s++=x, j=N; --j; *s++ = (x*=69069U) & 0xFFFFFFFFU);
};

uint reloadMT(void)
{
    register uint *p0=state, *p2=state+2, *pM=state+M, s0, s1;
    register int j;
    if(left < -1) seedMT(4357U);
    left=N-1, next=state+1;
    for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    for(pM=state, j=M; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1=state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1 ^= (s1 >> 11);
    s1 ^= (s1 <<  7) & 0x9D2C5680U;
    s1 ^= (s1 << 15) & 0xEFC60000U;
    return(s1 ^ (s1 >> 18));
};

uint randomMT(void)
{
    uint y;
    if(--left < 0) return(reloadMT());
    y  = *next++;
    y ^= (y >> 11);
    y ^= (y <<  7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    return(y ^ (y >> 18));
};
