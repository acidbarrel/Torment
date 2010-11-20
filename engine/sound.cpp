// sound.cpp: uses fmod on windows and sdl_mixer on unix (both had problems on the other platform)

#include "pch.h"
#include "engine.h"

//#ifndef WIN32    // NOTE: fmod not being supported for the moment as it does not allow stereo pan/vol updating during playback
#define USE_MIXER
//#endif

bool nosound = true;

#define MAXCHAN 32
#define SOUNDFREQ 22050

#ifdef USE_MIXER
    #include "SDL_mixer.h"
    #define MAXVOL MIX_MAX_VOLUME
    Mix_Music *mod = NULL;
    void *stream = NULL;    // TODO
#else
    #include "fmod.h"
    FMUSIC_MODULE *mod = NULL;
    FSOUND_STREAM *stream = NULL;

    #define MAXVOL 255
    int musicchan;
#endif

struct sample
{
    char *name;
    int vol;
    int id;
    #ifdef USE_MIXER
            Mix_Chunk *sound;
    #else
            FSOUND_SAMPLE * sound;
    #endif

    sample() : name(NULL) {};
    ~sample() { DELETEA(name); };
};

struct soundloc { vec loc; bool inuse; sample *s; } soundlocs[MAXCHAN];

void setmusicvol(int musicvol)
{
    if(nosound) return;
    #ifdef USE_MIXER
        if(mod) Mix_VolumeMusic((musicvol*MAXVOL)/255);
    #else
        if(mod) FMUSIC_SetMasterVolume(mod, musicvol);
        else if(stream && musicchan>=0) FSOUND_SetVolume(musicchan, (musicvol*MAXVOL)/255);
    #endif
};

VARP(soundvol, 0, 255, 255);
VARFP(musicvol, 0, 128, 255, setmusicvol(musicvol));

char *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicdonecmd);
    if(mod)
    {
        #ifdef USE_MIXER
            Mix_HaltMusic();
            Mix_FreeMusic(mod);
        #else
            FMUSIC_FreeSong(mod);
        #endif
        mod = NULL;
    };
    if(stream)
    {
        #ifndef USE_MIXER
            FSOUND_Stream_Close(stream);
        #endif
        stream = NULL;
    };
};

VAR(soundbufferlen, 128, 1024, 4096);

void initsound()
{
    memset(soundlocs, 0, sizeof(soundloc)*MAXCHAN);
    #ifdef USE_MIXER
        if(Mix_OpenAudio(SOUNDFREQ, MIX_DEFAULT_FORMAT, 2, soundbufferlen)<0)
        {
            conoutf("sound init failed (SDL_mixer): %s", (size_t)Mix_GetError());
            return;
        };
	    Mix_AllocateChannels(MAXCHAN);	
    #else
        if(FSOUND_GetVersion()<FMOD_VERSION) fatal("old FMOD dll");
        if(!FSOUND_Init(SOUNDFREQ, MAXCHAN, FSOUND_INIT_GLOBALFOCUS))
        {
            conoutf("sound init failed (FMOD): %d", FSOUND_GetError());
            return;
        };
    #endif
    nosound = false;
};

void musicdone()
{
#ifdef USE_MIXER
    if(mod) Mix_FreeMusic(mod);
#else
    if(mod) FMUSIC_FreeSong(mod);
    if(stream) FSOUND_Stream_Close(stream);
#endif
    mod = NULL;
    stream = NULL;
    if(musicdonecmd)
    {
        char *cmd = musicdonecmd;
        musicdonecmd = NULL;
        execute(cmd);
        delete[] cmd;
    };
};

void music(char *name, char *cmd)
{
    if(nosound) return;
    stopsound();
    if(soundvol && musicvol)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);
        string sn;
        s_strcpy(sn, "packages/");
        s_strcat(sn, name);
        #ifdef USE_MIXER
            if((mod = Mix_LoadMUS(path(sn))))
            {
                Mix_PlayMusic(mod, cmd[0] ? 0 : -1);
                Mix_VolumeMusic((musicvol*MAXVOL)/255);
            }
        #else
            if((mod = FMUSIC_LoadSong(path(sn))))
            {
                FMUSIC_PlaySong(mod);
                FMUSIC_SetMasterVolume(mod, musicvol);
                FMUSIC_SetLooping(mod, cmd[0] ? FALSE : TRUE);
            }
            else if((stream = FSOUND_Stream_Open(path(sn), cmd[0] ? FSOUND_LOOP_OFF : FSOUND_LOOP_NORMAL, 0, 0)))
            {
                musicchan = FSOUND_Stream_Play(FSOUND_FREE, stream);
                if(musicchan>=0) { FSOUND_SetVolume(musicchan, (musicvol*MAXVOL)/255); FSOUND_SetPaused(musicchan, false); };
            }
        #endif
            else
            {
                conoutf("could not play music: %s", sn);
            };
    };
};

COMMAND(music, "ss");

hashtable<char *, sample> samples;
vector<sample *> samplevec;

int findsound(char *name, int vol)
{
    sample *s = samples.access(name);
    if(s) return s->id;
    char *n = newstring(name);
    s = &samples[n];
    samplevec.add(s);
    s->name = n;
    s->sound = NULL;
    s->vol = vol;
    if(!s->vol) s->vol = 100;
    return s->id = samplevec.length()-1;
};

int registersound(char *name, char *vol) { return findsound(name, atoi(vol)); };
COMMAND(registersound, "ss");

void clear_sound()
{
    if(nosound) return;
    stopsound();
    samplevec.setsizenodelete(0);
    samples.clear();
    #ifdef USE_MIXER
        Mix_CloseAudio();
    #else
        FSOUND_Close();
    #endif
};

VAR(stereo, 0, 1, 1);

void updatechanvol(int chan, const vec *loc, int svol)
{
    int vol = soundvol, pan = 255/2;
    if(loc)
    {
        vec v;
        float dist = camera1->o.dist(*loc, v);
        vol -= (int)(dist*3/4*soundvol/255); // simple mono distance attenuation
        if(vol<0) vol = 0;
        if(stereo && (v.x != 0 || v.y != 0))
        {
            float yaw = -atan2f(v.x, v.y) - camera1->yaw*RAD; // relative angle of sound along X-Y axis
            pan = int(255.9f*(0.5f*sinf(yaw)+0.5f)); // range is from 0 (left) to 255 (right)
        };
    };
    vol = (vol*MAXVOL*svol)/255/255;
    #ifdef USE_MIXER
        Mix_Volume(chan, vol);
        Mix_SetPanning(chan, 255-pan, pan);
    #else
        FSOUND_SetVolume(chan, vol);
        FSOUND_SetPan(chan, pan);
    #endif
};  

void newsoundloc(int chan, const vec *loc, sample *s)
{
    ASSERT(chan>=0 && chan<MAXCHAN);
    soundlocs[chan].loc = *loc;
    soundlocs[chan].inuse = true;
    soundlocs[chan].s = s;
};

void updatevol()
{
    if(nosound) return;
    loopi(MAXCHAN) if(soundlocs[i].inuse)
    {
        #ifdef USE_MIXER
            if(Mix_Playing(i))
        #else
            if(FSOUND_IsPlaying(i))
        #endif
                updatechanvol(i, &soundlocs[i].loc, soundlocs[i].s->vol);
            else soundlocs[i].inuse = false;
    };
#ifndef USE_MIXER
    if(mod && FMUSIC_IsFinished(mod)) musicdone();
    else if(stream && !FSOUND_IsPlaying(musicchan)) musicdone();
#else
    if(mod && !Mix_PlayingMusic()) musicdone();
#endif
};

int soundsatonce = 0, lastsoundmillis = 0;

void playsound(int n, const vec *loc)
{
    if(nosound) return;
    if(!soundvol) return;
    if(lastmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
    lastsoundmillis = lastmillis;
    if(soundsatonce>5) return;  // avoid bursts of sounds with heavy packetloss and in sp
    if(!samplevec.inrange(n)) { conoutf("unregistered sound: %d", n); return; };

    if(!samplevec[n]->sound)
    {
        s_sprintfd(buf)("packages/sounds/%s.wav", samplevec[n]->name);

        #ifdef USE_MIXER
            samplevec[n]->sound = Mix_LoadWAV(path(buf));
        #else
            samplevec[n]->sound = FSOUND_Sample_Load(n, path(buf), FSOUND_LOOP_OFF, 0, 0);
        #endif

        if(!samplevec[n]->sound) { conoutf("failed to load sample: %s", buf); return; };
    };

    #ifdef USE_MIXER
        int chan = Mix_PlayChannel(-1, samplevec[n]->sound, 0);
    #else
        int chan = FSOUND_PlaySoundEx(FSOUND_FREE, samplevec[n]->sound, NULL, true);
    #endif
    if(chan<0) return;
    if(loc) newsoundloc(chan, loc, samplevec[n]);
    updatechanvol(chan, loc, samplevec[n]->vol);
    #ifndef USE_MIXER
        FSOUND_SetPaused(chan, false);
    #endif
};

void playsoundname(char *s, const vec *loc, int vol) { playsound(findsound(s, vol), loc); }

void sound(int *n) { playsound(*n, NULL); };
COMMAND(sound, "i");


