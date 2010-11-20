// GL_ARB_vertex_program, GL_ARB_fragment_program
extern PFNGLGENPROGRAMSARBPROC            glGenPrograms_;
extern PFNGLBINDPROGRAMARBPROC            glBindProgram_;
extern PFNGLPROGRAMSTRINGARBPROC          glProgramString;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC  glProgramEnvParameter4f_;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fv_;

extern int renderpath;

enum { R_FIXEDFUNCTION = 0, R_ASMSHADER, /* R_GLSLANG */ };

enum { SHPARAM_VERTEX = 0, SHPARAM_PIXEL };

#define MAXSHADERPARAMS 10

struct ShaderParam
{
    int type;
    int index;
    float val[4];
};

enum { SHADER_DEFAULT = 0, SHADER_NORMALSLMS };

extern int shaderdetail;

struct Shader
{
    char *name;
    int type;
    GLuint vs, ps;
    vector<ShaderParam> defaultparams;
    Shader *fastshader;
    int fastdetail;

    void bindprograms()
    {
        glBindProgram_(GL_VERTEX_PROGRAM_ARB,   vs);
        glBindProgram_(GL_FRAGMENT_PROGRAM_ARB, ps);
    };

    void set()
    {
        if(renderpath==R_FIXEDFUNCTION) return;
        if(fastshader && shaderdetail <= fastdetail) fastshader->bindprograms();
        else bindprograms();
    };
};

extern Shader *defaultshader;
extern Shader *notextureshader;
extern Shader *nocolorshader;

extern Shader *lookupshaderbyname(const char *name);

