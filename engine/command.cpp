// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "pch.h"
#include "engine.h"
#ifndef WIN32
#include <dirent.h>
#endif

void itoa(char *s, int i) { s_sprintf(s)("%d", i); };
char *exchangestr(char *o, const char *n) { delete[] o; return newstring(n); };

typedef hashtable<char *, ident> identtable;

identtable *idents = NULL;        // contains ALL vars/commands/aliases

bool overrideidents = false, persistidents = true;

void clearstack(ident &id)
{
    identstack *stack = id._stack;
    while(stack)
    {
        delete[] stack->action;
        identstack *tmp = stack;
        stack = stack->next;
        delete tmp;
    };
    id._stack = NULL;
};

void clear_command()
{
    enumerate(*idents, ident, i, if(i._type==ID_ALIAS) { DELETEA(i._name); DELETEA(i._action); if(i._stack) clearstack(i); });
    if(idents) idents->clear();
};

void clearoverrides()
{
    enumerate(*idents, ident, i,
        if(i._override!=NO_OVERRIDE)
        {
            switch(i._type)
            {
                case ID_ALIAS: 
                    if(i._action[0]) i._action = exchangestr(i._action, ""); 
                    break;
                case ID_VAR: 
                    *i._storage = i._override;
                    if(i._fun) ((void (__cdecl *)())i._fun)();
                    break;
            };
            i._override = NO_OVERRIDE;
        });
};

void pushident(ident &id, char *val)
{
    identstack *stack = new identstack;
    stack->action = id._isexecuting==id._action ? newstring(id._action) : id._action;
    stack->next = id._stack;
    id._stack = stack;
    id._action = val;
};

void popident(ident &id)
{
    if(!id._stack) return;
    if(id._action != id._isexecuting) delete[] id._action;
    identstack *stack = id._stack;
    id._action = stack->action;
    id._stack = stack->next;
    delete stack;
};

void pusha(char *name, char *action)
{
    ident *id = idents->access(name);
    if(!id)
    {
        name = newstring(name);
        ident init(ID_ALIAS, name, newstring(""), persistidents);
        id = idents->access(name, &init);
    };
    pushident(*id, action);
};

void push(char *name, char *action)
{
    pusha(name, newstring(action));
};

void pop(char *name)
{
    ident *id = idents->access(name);
    if(id) popident(*id);
};

COMMAND(push, "ss");
COMMAND(pop, "s");

void aliasa(char *name, char *action)
{
    ident *b = idents->access(name);
    if(!b) 
    {
        name = newstring(name);
        ident b(ID_ALIAS, name, action, persistidents);
        if(overrideidents) b._override = OVERRIDDEN;
        idents->access(name, &b);
    }
    else if(b->_type != ID_ALIAS)
    {
        conoutf("cannot redefine builtin %s with an alias", name);
        delete[] action;
    }
    else 
    {
        if(b->_action != b->_isexecuting) delete[] b->_action;
        b->_action = action;
        if(overrideidents) b->_override = OVERRIDDEN;
        else 
        {
            if(b->_override != NO_OVERRIDE) b->_override = NO_OVERRIDE;
            if(b->_persist != persistidents) b->_persist = persistidents;
        };
    };
};

void alias(char *name, char *action) { aliasa(name, newstring(action)); };

COMMAND(alias, "ss");

// variable's and commands are registered through globals, see cube.h

int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist)
{
    if(!idents) idents = new identtable;
    ident v(ID_VAR, name, min, cur, max, storage, (void *)fun, persist);
    idents->access(name, &v);
    return cur;
};

#define GETVAR(id, name, retval) \
    ident *id = idents->access(name); \
    if(!id || id->_type!=ID_VAR) return retval;
void setvar(char *name, int i, bool dofunc) 
{ 
    GETVAR(id, name, );
    *id->_storage = i; 
    if(dofunc && id->_fun) ((void (__cdecl *)())id->_fun)();
}; 
int getvar(char *name) 
{ 
    GETVAR(id, name, 0);
    return *id->_storage;
};
int getvarmin(char *name) 
{ 
    GETVAR(id, name, 0);
    return id->_min;
};
int getvarmax(char *name) 
{ 
    GETVAR(id, name, 0);
    return id->_max;
};
bool identexists(char *name) { return idents->access(name)!=NULL; };
ident *getident(char *name) { return idents->access(name); };

const char *getalias(char *name)
{
    ident *i = idents->access(name);
    return i && i->_type==ID_ALIAS ? i->_action : "";
};

bool addcommand(char *name, void (*fun)(), char *narg)
{
    if(!idents) idents = new identtable;
    ident c(ID_COMMAND, name, narg, (void *)fun);
    idents->access(name, &c);
    return false;
};

void addident(char *name, ident *id)
{
    if(!idents) idents = new identtable;
    idents->access(name, id);
};

static vector<vector<char> > wordbufs;
static int bufnest = 0;

char *parseexp(char *&p, int right);

void parsemacro(char *&p, int level, vector<char> &wordbuf)
{
    int escape = 1;
    while(*p=='@') p++, escape++;
    if(level > escape)
    {
        while(escape--) wordbuf.add('@');
        return;
    };
    if(*p=='(')
    {
        char *ret = parseexp(p, ')');
        if(ret)
        {
            for(char *sub = ret; *sub; ) wordbuf.add(*sub++);
            delete[] ret;
        };
        return;
    };
    char *ident = p;
    while(isalnum(*p) || *p=='_') p++;
    int c = *p;
    *p = 0;
    const char *alias = getalias(ident);
    *p = c;
    while(*alias) wordbuf.add(*alias++);
};

char *parseexp(char *&p, int right)          // parse any nested set of () or []
{
    if(bufnest++>=wordbufs.length()) wordbufs.add();
    vector<char> &wordbuf = wordbufs[bufnest-1];
    int left = *p++;
    for(int brak = 1; brak; )
    {
        int c = *p++;
        if(c=='\r') continue;               // hack
        if(left=='[' && c=='@')
        {
            parsemacro(p, brak, wordbuf);
            continue;
        };
        if(c=='\"')
        {
            wordbuf.add(c);
            char *end = p+strcspn(p, "\"\r\n\0");
            while(p < end) wordbuf.add(*p++);
            if(*p=='\"') wordbuf.add(*p++);
            continue;
        };
        if(c=='/' && *p=='/')
        {
            p += strcspn(p, "\n\0");
            continue;
        };
            
        if(c==left) brak++;
        else if(c==right) brak--;
        else if(!c) 
        { 
            p--;
            conoutf("missing \"%c\"", right);
            wordbuf.setsize(0); 
            bufnest--;
            return NULL; 
        };
        wordbuf.add(c);
    };
    wordbuf.pop();
    char *s;
    if(left=='(')
    {
        wordbuf.add(0);
        char *ret = executeret(wordbuf.getbuf());                    // evaluate () exps directly, and substitute result
        wordbuf.pop();
        s = ret ? ret : newstring("");
    }
    else
    {
        s = newstring(wordbuf.getbuf(), wordbuf.length());
    };
    wordbuf.setsize(0);
    bufnest--;
    return s;
};

char *lookup(char *n)                           // find value of ident referenced with $ in exp
{
    ident *id = idents->access(n+1);
    if(id) switch(id->_type)
    {
        case ID_VAR: { string t; itoa(t, *(id->_storage)); return exchangestr(n, t); };
        case ID_ALIAS: return exchangestr(n, id->_action);
    };
    conoutf("unknown alias lookup: %s", n+1);
    return n;
};

char *parseword(char *&p)                       // parse single argument, including expressions
{
    for(;;)
    {
        p += strspn(p, " \t\r");
        if(p[0]!='/' || p[1]!='/') break;
        p += strcspn(p, "\n\0");  
    };
    if(*p=='\"')
    {
        p++;
        char *word = p;
        p += strcspn(p, "\"\r\n\0");
        char *s = newstring(word, p-word);
        if(*p=='\"') p++;
        return s;
    };
    if(*p=='(') return parseexp(p, ')');
    if(*p=='[') return parseexp(p, ']');
    char *word = p;
    for(;;)
    {
        p += strcspn(p, "/; \t\r\n\0");
        if(p[0]!='/' || p[1]=='/') break;
        else if(p[1]=='\0') { p++; break; };
        p += 2;
    };
    if(p-word==0) return NULL;
    char *s = newstring(word, p-word);
    if(*s=='$') return lookup(s);               // substitute variables
    return s;
};

char *conc(char **w, int n, bool space)
{
    int len = space ? max(n-1, 0) : 0;
    loopj(n) len += (int)strlen(w[j]);
    char *r = newstring("", len);
    loopi(n)       
    {
        strcat(r, w[i]);  // make string-list out of all arguments
        if(i==n-1) break;
        if(space) strcat(r, " ");
    };
    return r;
};

VARN(numargs, _numargs, 0, 0, 25);

#define parseint(s) strtol((s), NULL, 0)

char *commandret = NULL;

extern const char *addreleaseaction(const char *s);

char *executeret(char *p)               // all evaluation happens here, recursively
{
    const int MAXWORDS = 25;                    // limit, remove
    char *w[MAXWORDS];
    char *retval = NULL;
    #define setretval(v) { char *rv = v; if(rv) retval = rv; commandret = NULL; }
    for(bool cont = true; cont;)                // for each ; seperated statement
    {
        int numargs = MAXWORDS;
        loopi(MAXWORDS)                         // collect all argument values
        {
            w[i] = "";
            if(i>numargs) continue;
            char *s = parseword(p);             // parse and evaluate exps
            if(s) w[i] = s;
            else numargs = i;
        };
        
        p += strcspn(p, ";\n\0");
        cont = *p++!=0;                         // more statements if this isn't the end of the string
        char *c = w[0];
        if(*c=='/') c++;                        // strip irc-style command prefix
        if(!*c) continue;                       // empty statement
        
        DELETEA(retval);

        if(w[1][0]=='=' && !w[1][1])
        {
            aliasa(c, numargs>2 ? w[2] : newstring(""));
            w[2] = NULL;
        }
        else
        {     
            ident *id = idents->access(c);
            if(!id)
            {
                if(!parseint(c) && *c!='0')
                    conoutf("unknown command: %s", c);
                setretval(newstring(c));
            }
            else switch(id->_type)
            {
                case ID_ICOMMAND:
                {
                    switch(id->_narg[0])
                    {
                        default: id->run(w+1); break;
                        case 'D': id->run((char **)addreleaseaction(id->_name)); break;
                        case 'C': 
                        { 
                            char *r = conc(w+1, numargs-1, true); 
                            id->run(&r); 
                            delete[] r; 
                            break;
                        };
                    };
                    setretval(commandret);
                    break;
                };

                case ID_COMMAND:                     // game defined commands
                {    
                    void *v[MAXWORDS];
                    union
                    {
                        int i;
                        float f;
                    } nstor[MAXWORDS];
                    int n = 0, wn = 0;
                    for(char *a = id->_narg; *a; a++) switch(*a)
                    {
                        case 's':                                 v[n] = w[++wn];     n++; break;
                        case 'i': nstor[n].i = parseint(w[++wn]); v[n] = &nstor[n].i; n++; break;
                        case 'f': nstor[n].f = atof(w[++wn]);     v[n] = &nstor[n].f; n++; break;
                        case 'D': nstor[n].i = addreleaseaction(id->_name) ? 1 : 0; v[n] = &nstor[n].i; n++; break;
                        case 'V': v[n++] = w+1; nstor[n].i = numargs-1; v[n] = &nstor[n].i; n++; break;
                        case 'C': v[n++] = conc(w+1, numargs-1, true);  break;
                        default: fatal("builtin declared with illegal type");
                    };
                    switch(n)
                    {
                        case 0: ((void (__cdecl *)()                                      )id->_fun)();                             break;
                        case 1: ((void (__cdecl *)(void *)                                )id->_fun)(v[0]);                         break;
                        case 2: ((void (__cdecl *)(void *, void *)                        )id->_fun)(v[0], v[1]);                   break;
                        case 3: ((void (__cdecl *)(void *, void *, void *)                )id->_fun)(v[0], v[1], v[2]);             break;
                        case 4: ((void (__cdecl *)(void *, void *, void *, void *)        )id->_fun)(v[0], v[1], v[2], v[3]);       break;
                        case 5: ((void (__cdecl *)(void *, void *, void *, void *, void *))id->_fun)(v[0], v[1], v[2], v[3], v[4]); break;
                        default: fatal("builtin declared with too many args (use V?)");
                    };
                    if(id->_narg[0]=='C') delete[] (char *)v[0];
                    setretval(commandret);
                    break;
                };

                case ID_VAR:                        // game defined variables 
                    if(!w[1][0]) conoutf("%s = %d", c, *id->_storage);      // var with no value just prints its current value
                    else
                    {
                        if(overrideidents)
                        {
                            if(id->_persist)
                            {
                                conoutf("cannot override persistent var %s", id->_name);
                                break;
                            };
                            if(id->_override==NO_OVERRIDE) id->_override = *id->_storage;
                        }
                        else if(id->_override!=NO_OVERRIDE) id->_override = NO_OVERRIDE;
                        int i1 = parseint(w[1]);
                        if(i1<id->_min || i1>id->_max)
                        {
                            i1 = i1<id->_min ? id->_min : id->_max;                // clamp to valid range
                            conoutf("valid range for %s is %d..%d", c, id->_min, id->_max);
                        }
                        *id->_storage = i1;
                        if(id->_fun) ((void (__cdecl *)())id->_fun)();            // call trigger function if available
                    };
                    break;
                    
                case ID_ALIAS:                              // alias, also used as functions and (global) variables
                {
                    for(int i = 1; i<numargs; i++)
                    {
                        s_sprintfd(t)("arg%d", i);          // set any arguments as (global) arg values so functions can access them
                        pusha(t, w[i]);
                        w[i] = NULL;
                    };
                    _numargs = numargs-1;
                    bool wasoverriding = overrideidents;
                    if(id->_override!=NO_OVERRIDE) overrideidents = true;
                    char *wasexecuting = id->_isexecuting;
                    id->_isexecuting = id->_action;
                    setretval(executeret(id->_action));
                    if(id->_isexecuting != id->_action && id->_isexecuting != wasexecuting) delete[] id->_isexecuting;
                    id->_isexecuting = wasexecuting;
                    overrideidents = wasoverriding;
                    for(int i = 1; i<numargs; i++)
                    {
                        s_sprintfd(t)("arg%d", i);          // set any arguments as (global) arg values so functions can access them
                        pop(t);
                    };
                    break;
                };
            };
        };
        loopj(numargs) if(w[j]) delete[] w[j];
    };
    return retval;
};

int execute(char *p)
{
    char *ret = executeret(p);
    int i = 0; 
    if(ret) { i = parseint(ret); delete[] ret; };
    return i;
};

// tab-completion of all idents and base maps

struct fileskey
{
    const char *dir, *ext;

    fileskey() {};
    fileskey(const char *dir, const char *ext) : dir(dir), ext(ext) {};
};

struct filesval
{
    char *dir, *ext;
    vector<char *> files;

    filesval(const char *dir, const char *ext) : dir(newstring(dir)), ext(ext[0] ? newstring(ext) : NULL) {};
    ~filesval() { DELETEA(dir); DELETEA(ext); loopv(files) DELETEA(files[i]); files.setsize(0); };
};

static inline bool htcmp(const fileskey &x, const fileskey &y)
{
    return !strcmp(x.dir, y.dir) && (x.ext == y.ext || (x.ext && y.ext && !strcmp(x.ext, y.ext)));
};

static inline uint hthash(const fileskey &k)
{
    return hthash(k.dir);
};

static hashtable<fileskey, filesval *> completefiles;
static hashtable<char *, filesval *> completions;

int completesize = 0;
string lastcomplete;

void resetcomplete() { completesize = 0; }

void addcomplete(char *command, char *dir, char *ext)
{
    if(overrideidents)
    {
        conoutf("cannot override complete %s", command);
        return;
    };
    if(!dir[0])
    {
        filesval **hasfiles = completions.access(command);
        if(hasfiles) *hasfiles = NULL;
        return;
    };
    int dirlen = (int)strlen(dir);
    while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
        dir[--dirlen] = '\0';
    if(strchr(ext, '*')) ext[0] = '\0';
    fileskey key(dir, ext[0] ? ext : NULL);
    filesval **val = completefiles.access(key);
    if(!val)
    {
        filesval *f = new filesval(dir, ext);
        val = &completefiles[fileskey(f->dir, f->ext)];
        *val = f;
    };
    filesval **hasfiles = completions.access(command);
    if(hasfiles) *hasfiles = *val;
    else completions[newstring(command)] = *val;
};

COMMANDN(complete, addcomplete, "sss");

void buildfilenames(filesval *f)
{
    int extsize = f->ext ? (int)strlen(f->ext)+1 : 0;
    #if defined(WIN32)
    s_sprintfd(pathname)("%s\\*.%s", f->dir, f->ext ? f->ext : "*");
    WIN32_FIND_DATA	FindFileData;
    HANDLE Find = FindFirstFile(path(pathname), &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            f->files.add(newstring(FindFileData.cFileName, (int)strlen(FindFileData.cFileName) - extsize));
        } while(FindNextFile(Find, &FindFileData));
    }
    #elif defined(__GNUC__)
    string pathname;
    s_strcpy(pathname, f->dir);
    DIR *d = opendir(path(pathname));
    if(d)
    {
        struct dirent *dir;
        while((dir = readdir(d)) != NULL)
        {
            if(!f->ext) f->files.add(newstring(dir->d_name));
            else
            {
                int namelength = strlen(dir->d_name) - extsize;
                if(namelength > 0 && dir->d_name[namelength] == '.' && strncmp(dir->d_name+namelength+1, f->ext, extsize-1)==0)
                    f->files.add(newstring(dir->d_name, namelength));
            };
        };
        closedir(d);
    }
    #else
    if(0)
    #endif
    else conoutf("unable to read base folder for map autocomplete");
};

void complete(char *s)
{
    if(*s!='/')
    {
        string t;
        s_strcpy(t, s);
        s_strcpy(s, "/");
        s_strcat(s, t);
    };
    if(!s[1]) return;
    if(!completesize) { completesize = (int)strlen(s)-1; lastcomplete[0] = '\0'; };

    filesval *f = NULL;
    if(completesize)
    {
        char *end = strchr(s, ' ');
        if(end)
        {
            string command;
            s_strncpy(command, s+1, min(size_t(end-s), sizeof(command)));
            filesval **hasfiles = completions.access(command);
            if(hasfiles) f = *hasfiles;
        };
    };

    char *nextcomplete = NULL;
    string prefix;
    s_strcpy(prefix, "/");
    if(f) // complete using filenames
    {
        int commandsize = strchr(s, ' ')+1-s; 
        s_strncpy(prefix, s, min(size_t(commandsize+1), sizeof(prefix)));
        if(f->files.empty()) buildfilenames(f);
        loopi(f->files.length())
        {
            if(strncmp(f->files[i], s+commandsize, completesize+1-commandsize)==0 && 
               strcmp(f->files[i], lastcomplete) > 0 && (!nextcomplete || strcmp(f->files[i], nextcomplete) < 0))
                nextcomplete = f->files[i];
        };
    }
    else // complete using command names
    {
        enumerate(*idents, ident, id,
            if(strncmp(id._name, s+1, completesize)==0 &&
               strcmp(id._name, lastcomplete) > 0 && (!nextcomplete || strcmp(id._name, nextcomplete) < 0))
                nextcomplete = id._name;
        );
    };
    if(nextcomplete)
    {
        s_strcpy(s, prefix);
        s_strcat(s, nextcomplete);
        s_strcpy(lastcomplete, nextcomplete);
    }
    else lastcomplete[0] = '\0';
};

bool execfile(char *cfgfile)
{
    string s;
    s_strcpy(s, cfgfile);
    char *buf = loadfile(path(s), NULL);
    if(!buf) return false;
    execute(buf);
    delete[] buf;
    return true;
};

void exec(char *cfgfile)
{
    if(!execfile(cfgfile)) conoutf("could not read \"%s\"", cfgfile);
};

void writecfg()
{
    FILE *f = fopen("config.cfg", "w");
    if(!f) return;
    fprintf(f, "// automatically written on exit, DO NOT MODIFY\n// delete this file to have defaults.cfg overwrite these settings\n// modify settings in game, or put settings in autoexec.cfg to override anything\n\n");
    cc->writeclientinfo(f);
    fprintf(f, "\n");
    enumerate(*idents, ident, id,
        if(id._type==ID_VAR && id._persist)
        {
            fprintf(f, "%s %d\n", id._name, *id._storage);
        };
    );
    fprintf(f, "\n");
    writebinds(f);
    fprintf(f, "\n");
    enumerate(*idents, ident, id,
        if(id._type==ID_ALIAS && id._persist && id._override==NO_OVERRIDE && !strstr(id._name, "nextmap_") && id._action[0])
        {
            fprintf(f, "\"%s\" = [%s]\n", id._name, id._action);
        };
    );
    fprintf(f, "\n");
    enumeratekt(completions, char *, k, filesval *, v,
        if(v) fprintf(f, "complete \"%s\" \"%s\" \"%s\"\n", k, v->dir, v->ext ? v->ext : "*");
    );
    fclose(f);
};

COMMAND(writecfg, "");

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

void intset(char *name, int v) { string b; itoa(b, v); alias(name, b); };
void intret            (int v) { string b; itoa(b, v); commandret = newstring(b); };

ICOMMAND(if, "sss", commandret = executeret(args[0][0]!='0' ? args[1] : args[2]));

ICOMMAND(loop, "sss", { int n = parseint(args[1]); loopi(n) { intset(args[0], i); execute(args[2]); }; });
ICOMMAND(while, "ss", while(execute(args[0])) execute(args[1]));    // can't get any simpler than this :)

void concat(const char *s) { commandret = newstring(s); };
void result(const char *s) { commandret = newstring(s); };

void concatword(char **args, int *numargs)
{
    commandret = conc(args, *numargs, false);
};

void format(char **args, int *numargs)
{
    vector<char> s;
    char *f = args[0];
    while(*f)
    {
        int c = *f++;
        if(c == '%')
        {
            int i = *f++;
            if(i >= '1' && i <= '9')
            {
                i -= '0';
                const char *sub = i < *numargs ? args[i] : "";
                while(*sub) s.add(*sub++);
            }
            else s.add(i);
        }
        else s.add(c);
    };
    s.add('\0');
    result(s.getbuf());
};

#define whitespaceskip s += strspn(s, "\n\t ")
#define elementskip *s=='"' ? (++s, s += strcspn(s, "\"\n\0"), s += *s=='"') : s += strcspn(s, "\n\t \0")

void listlen(char *s)
{
    int n = 0;
    whitespaceskip;
    for(; *s; n++) elementskip, whitespaceskip;
    intret(n);
};

void at(char *s, int *pos)
{
    whitespaceskip;
    loopi(*pos) elementskip, whitespaceskip;
    char *e = s;
    elementskip;
    if(*e=='"') 
    {
        e++;
        if(s[-1] == '"') --s;
    };
    *s = '\0';
    result(e);
};

void getalias_(char *s)
{
    result(getalias(s));
};

COMMAND(exec, "s");
COMMAND(concat, "C");
COMMAND(result, "s");
COMMAND(concatword, "V");
COMMAND(format, "V");
COMMAND(at, "si");
COMMAND(listlen, "s");
COMMANDN(getalias, getalias_, "s");

void add  (int *a, int *b) { intret(*a + *b); };          COMMANDN(+, add, "ii");
void mul  (int *a, int *b) { intret(*a * *b); };          COMMANDN(*, mul, "ii");
void sub  (int *a, int *b) { intret(*a - *b); };          COMMANDN(-, sub, "ii");
void divi (int *a, int *b) { intret(*b ? *a / *b : 0); }; COMMANDN(div, divi, "ii");
void mod  (int *a, int *b) { intret(*b ? *a % *b : 0); }; COMMAND(mod, "ii");
void equal(int *a, int *b) { intret((int)(*a == *b)); };  COMMANDN(=, equal, "ii");
void nequal(int *a, int *b) { intret((int)(*a != *b)); }; COMMANDN(!=, nequal, "ii");
void lt   (int *a, int *b) { intret((int)(*a < *b)); };   COMMANDN(<, lt, "ii");
void gt   (int *a, int *b) { intret((int)(*a > *b)); };   COMMANDN(>, gt, "ii");
void lte   (int *a, int *b) { intret((int)(*a <= *b)); }; COMMANDN(<=, lte, "ii");
void gte   (int *a, int *b) { intret((int)(*a >= *b)); }; COMMANDN(>=, gte, "ii");
void xora (int *a, int *b) { intret(*a ^ *b); };          COMMANDN(^, xora, "ii");
void nota (int *a)         { intret(*a == 0); };          COMMANDN(!, nota, "i");

void anda (char *a, char *b) { intret(execute(a)!=0 && execute(b)!=0); };
void ora  (char *a, char *b) { intret(execute(a)!=0 || execute(b)!=0); };

COMMANDN(&&, anda, "ss");
COMMANDN(||, ora, "ss");

void rndn(int *a)          { intret(*a>0 ? rnd(*a) : 0); };  COMMANDN(rnd, rndn, "i");

void strcmpa(char *a, char *b) { intret(strcmp(a,b)==0); };  COMMANDN(strcmp, strcmpa, "ss");

ICOMMAND(echo, "C", conoutf("\f1%s", args[0]));

void strstra(char *a, char *b) { char *s = strstr(a, b); intret(s ? s-a : -1); }; COMMANDN(strstr, strstra, "ss");

