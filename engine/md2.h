struct md2;

md2 *loadingmd2 = 0;

float md2normaltable[256][3] =
{
    { -0.525731f,  0.000000f,  0.850651f },     { -0.442863f,  0.238856f,  0.864188f },     { -0.295242f,  0.000000f,  0.955423f },     { -0.309017f,  0.500000f,  0.809017f }, 
    { -0.162460f,  0.262866f,  0.951056f },     {  0.000000f,  0.000000f,  1.000000f },     {  0.000000f,  0.850651f,  0.525731f },     { -0.147621f,  0.716567f,  0.681718f }, 
    {  0.147621f,  0.716567f,  0.681718f },     {  0.000000f,  0.525731f,  0.850651f },     {  0.309017f,  0.500000f,  0.809017f },     {  0.525731f,  0.000000f,  0.850651f }, 
    {  0.295242f,  0.000000f,  0.955423f },     {  0.442863f,  0.238856f,  0.864188f },     {  0.162460f,  0.262866f,  0.951056f },     { -0.681718f,  0.147621f,  0.716567f }, 
    { -0.809017f,  0.309017f,  0.500000f },     { -0.587785f,  0.425325f,  0.688191f },     { -0.850651f,  0.525731f,  0.000000f },     { -0.864188f,  0.442863f,  0.238856f }, 
    { -0.716567f,  0.681718f,  0.147621f },     { -0.688191f,  0.587785f,  0.425325f },     { -0.500000f,  0.809017f,  0.309017f },     { -0.238856f,  0.864188f,  0.442863f }, 
    { -0.425325f,  0.688191f,  0.587785f },     { -0.716567f,  0.681718f, -0.147621f },     { -0.500000f,  0.809017f, -0.309017f },     { -0.525731f,  0.850651f,  0.000000f }, 
    {  0.000000f,  0.850651f, -0.525731f },     { -0.238856f,  0.864188f, -0.442863f },     {  0.000000f,  0.955423f, -0.295242f },     { -0.262866f,  0.951056f, -0.162460f }, 
    {  0.000000f,  1.000000f,  0.000000f },     {  0.000000f,  0.955423f,  0.295242f },     { -0.262866f,  0.951056f,  0.162460f },     {  0.238856f,  0.864188f,  0.442863f }, 
    {  0.262866f,  0.951056f,  0.162460f },     {  0.500000f,  0.809017f,  0.309017f },     {  0.238856f,  0.864188f, -0.442863f },     {  0.262866f,  0.951056f, -0.162460f }, 
    {  0.500000f,  0.809017f, -0.309017f },     {  0.850651f,  0.525731f,  0.000000f },     {  0.716567f,  0.681718f,  0.147621f },     {  0.716567f,  0.681718f, -0.147621f }, 
    {  0.525731f,  0.850651f,  0.000000f },     {  0.425325f,  0.688191f,  0.587785f },     {  0.864188f,  0.442863f,  0.238856f },     {  0.688191f,  0.587785f,  0.425325f }, 
    {  0.809017f,  0.309017f,  0.500000f },     {  0.681718f,  0.147621f,  0.716567f },     {  0.587785f,  0.425325f,  0.688191f },     {  0.955423f,  0.295242f,  0.000000f }, 
    {  1.000000f,  0.000000f,  0.000000f },     {  0.951056f,  0.162460f,  0.262866f },     {  0.850651f, -0.525731f,  0.000000f },     {  0.955423f, -0.295242f,  0.000000f }, 
    {  0.864188f, -0.442863f,  0.238856f },     {  0.951056f, -0.162460f,  0.262866f },     {  0.809017f, -0.309017f,  0.500000f },     {  0.681718f, -0.147621f,  0.716567f }, 
    {  0.850651f,  0.000000f,  0.525731f },     {  0.864188f,  0.442863f, -0.238856f },     {  0.809017f,  0.309017f, -0.500000f },     {  0.951056f,  0.162460f, -0.262866f }, 
    {  0.525731f,  0.000000f, -0.850651f },     {  0.681718f,  0.147621f, -0.716567f },     {  0.681718f, -0.147621f, -0.716567f },     {  0.850651f,  0.000000f, -0.525731f }, 
    {  0.809017f, -0.309017f, -0.500000f },     {  0.864188f, -0.442863f, -0.238856f },     {  0.951056f, -0.162460f, -0.262866f },     {  0.147621f,  0.716567f, -0.681718f }, 
    {  0.309017f,  0.500000f, -0.809017f },     {  0.425325f,  0.688191f, -0.587785f },     {  0.442863f,  0.238856f, -0.864188f },     {  0.587785f,  0.425325f, -0.688191f }, 
    {  0.688191f,  0.587785f, -0.425325f },     { -0.147621f,  0.716567f, -0.681718f },     { -0.309017f,  0.500000f, -0.809017f },     {  0.000000f,  0.525731f, -0.850651f }, 
    { -0.525731f,  0.000000f, -0.850651f },     { -0.442863f,  0.238856f, -0.864188f },     { -0.295242f,  0.000000f, -0.955423f },     { -0.162460f,  0.262866f, -0.951056f }, 
    {  0.000000f,  0.000000f, -1.000000f },     {  0.295242f,  0.000000f, -0.955423f },     {  0.162460f,  0.262866f, -0.951056f },     { -0.442863f, -0.238856f, -0.864188f }, 
    { -0.309017f, -0.500000f, -0.809017f },     { -0.162460f, -0.262866f, -0.951056f },     {  0.000000f, -0.850651f, -0.525731f },     { -0.147621f, -0.716567f, -0.681718f }, 
    {  0.147621f, -0.716567f, -0.681718f },     {  0.000000f, -0.525731f, -0.850651f },     {  0.309017f, -0.500000f, -0.809017f },     {  0.442863f, -0.238856f, -0.864188f }, 
    {  0.162460f, -0.262866f, -0.951056f },     {  0.238856f, -0.864188f, -0.442863f },     {  0.500000f, -0.809017f, -0.309017f },     {  0.425325f, -0.688191f, -0.587785f }, 
    {  0.716567f, -0.681718f, -0.147621f },     {  0.688191f, -0.587785f, -0.425325f },     {  0.587785f, -0.425325f, -0.688191f },     {  0.000000f, -0.955423f, -0.295242f }, 
    {  0.000000f, -1.000000f,  0.000000f },     {  0.262866f, -0.951056f, -0.162460f },     {  0.000000f, -0.850651f,  0.525731f },     {  0.000000f, -0.955423f,  0.295242f }, 
    {  0.238856f, -0.864188f,  0.442863f },     {  0.262866f, -0.951056f,  0.162460f },     {  0.500000f, -0.809017f,  0.309017f },     {  0.716567f, -0.681718f,  0.147621f }, 
    {  0.525731f, -0.850651f,  0.000000f },     { -0.238856f, -0.864188f, -0.442863f },     { -0.500000f, -0.809017f, -0.309017f },     { -0.262866f, -0.951056f, -0.162460f }, 
    { -0.850651f, -0.525731f,  0.000000f },     { -0.716567f, -0.681718f, -0.147621f },     { -0.716567f, -0.681718f,  0.147621f },     { -0.525731f, -0.850651f,  0.000000f }, 
    { -0.500000f, -0.809017f,  0.309017f },     { -0.238856f, -0.864188f,  0.442863f },     { -0.262866f, -0.951056f,  0.162460f },     { -0.864188f, -0.442863f,  0.238856f }, 
    { -0.809017f, -0.309017f,  0.500000f },     { -0.688191f, -0.587785f,  0.425325f },     { -0.681718f, -0.147621f,  0.716567f },     { -0.442863f, -0.238856f,  0.864188f }, 
    { -0.587785f, -0.425325f,  0.688191f },     { -0.309017f, -0.500000f,  0.809017f },     { -0.147621f, -0.716567f,  0.681718f },     { -0.425325f, -0.688191f,  0.587785f }, 
    { -0.162460f, -0.262866f,  0.951056f },     {  0.442863f, -0.238856f,  0.864188f },     {  0.162460f, -0.262866f,  0.951056f },     {  0.309017f, -0.500000f,  0.809017f }, 
    {  0.147621f, -0.716567f,  0.681718f },     {  0.000000f, -0.525731f,  0.850651f },     {  0.425325f, -0.688191f,  0.587785f },     {  0.587785f, -0.425325f,  0.688191f }, 
    {  0.688191f, -0.587785f,  0.425325f },     { -0.955423f,  0.295242f,  0.000000f },     { -0.951056f,  0.162460f,  0.262866f },     { -1.000000f,  0.000000f,  0.000000f }, 
    { -0.850651f,  0.000000f,  0.525731f },     { -0.955423f, -0.295242f,  0.000000f },     { -0.951056f, -0.162460f,  0.262866f },     { -0.864188f,  0.442863f, -0.238856f }, 
    { -0.951056f,  0.162460f, -0.262866f },     { -0.809017f,  0.309017f, -0.500000f },     { -0.864188f, -0.442863f, -0.238856f },     { -0.951056f, -0.162460f, -0.262866f }, 
    { -0.809017f, -0.309017f, -0.500000f },     { -0.681718f,  0.147621f, -0.716567f },     { -0.681718f, -0.147621f, -0.716567f },     { -0.850651f,  0.000000f, -0.525731f }, 
    { -0.688191f,  0.587785f, -0.425325f },     { -0.587785f,  0.425325f, -0.688191f },     { -0.425325f,  0.688191f, -0.587785f },     { -0.425325f, -0.688191f, -0.587785f }, 
    { -0.587785f, -0.425325f, -0.688191f },     { -0.688191f, -0.587785f, -0.425325f }
};

struct md2 : vertmodel
{
    struct md2_header
    {
        int magic;
        int version;
        int skinwidth, skinheight;
        int framesize;
        int numskins, numvertices, numtexcoords;
        int numtriangles, numglcommands, numframes;
        int offsetskins, offsettexcoords, offsettriangles;
        int offsetframes, offsetglcommands, offsetend;
    };

    struct md2_vertex
    {
        uchar vertex[3], normalindex;
    };

    struct md2_frame
    {
        float      scale[3];
        float      translate[3];
        char       name[16];
    };
    
    md2(const char *name) : vertmodel(name) {};

    int type() { return MDL_MD2; };

    struct md2part : part
    {
        void gentcverts(int *glcommands, vector<tcvert> &tcverts, vector<tri> &tris)
        {
            hashtable<ivec, int> tchash;
            vector<ushort> idxs;
            for(int *command = glcommands; (*command)!=0;)
            {
                int numvertex = *command++;
                bool isfan;
                if(isfan = (numvertex<0)) numvertex = -numvertex;
                idxs.setsizenodelete(0);
                loopi(numvertex)
                {
                    union { int i; float f; } u, v;
                    u.i = *command++;
                    v.i = *command++;
                    int vindex = *command++;
                    ivec tckey(u.i, v.i, vindex);
                    int *idx = tchash.access(tckey);
                    if(!idx)
                    {
                        idx = &tchash[tckey];
                        *idx = tcverts.length();
                        tcvert &tc = tcverts.add();
                        tc.u = u.f;
                        tc.v = v.f;
                        tc.index = (ushort)vindex;
                    };        
                    idxs.add(*idx);
                };
                loopi(numvertex-2) 
                { 
                    tri &t = tris.add();
                    if(isfan)
                    {
                        t.vert[0] = idxs[0];
                        t.vert[1] = idxs[i+1];
                        t.vert[2] = idxs[i+2];
                    }
                    else loopk(3) t.vert[k] = idxs[i&1 && k ? i+(1-(k-1))+1 : i+k];
                };
            };
        };
        
        bool load(char *filename)
        {
            if(loaded) return true;
            FILE *file = fopen(filename, "rb");
            if(!file) return false;

            show_out_of_renderloop_progress(0, filename);

            md2_header header;
            fread(&header, sizeof(md2_header), 1, file);
            endianswap(&header, sizeof(int), sizeof(md2_header)/sizeof(int));

            if(header.magic!=844121161 || header.version!=8) 
            {
                fclose(file);
                return false;
            };
           
            numframes = header.numframes;

            mesh &m = *new mesh;
            meshes.add(&m);

            int *glcommands = new int[header.numglcommands];
            fseek(file, header.offsetglcommands, SEEK_SET); 
            int numglcommands = fread(glcommands, sizeof(int), header.numglcommands, file);
            endianswap(glcommands, sizeof(int), numglcommands);
            if(numglcommands < header.numglcommands) memset(&glcommands[numglcommands], 0, (header.numglcommands-numglcommands)*sizeof(int));

            vector<tcvert> tcgen;
            vector<tri> trigen;
            gentcverts(glcommands, tcgen, trigen);
            delete[] glcommands;

            m.numtcverts = tcgen.length();
            m.tcverts = new tcvert[m.numtcverts];
            memcpy(m.tcverts, tcgen.getbuf(), m.numtcverts*sizeof(tcvert));
            m.numtris = trigen.length();
            m.tris = new tri[m.numtris];
            memcpy(m.tris, trigen.getbuf(), m.numtris*sizeof(tri));

            m.numverts = header.numvertices;
            m.verts = new vert[m.numverts*numframes];

            int frame_offset = header.offsetframes;
            vert *curvert = m.verts;
            loopi(header.numframes)
            {
                md2_frame frame;
                fseek(file, frame_offset, SEEK_SET);
                fread(&frame, sizeof(md2_frame), 1, file);
                endianswap(&frame, sizeof(float), 6);
                loopj(header.numvertices)
                {
                    md2_vertex v;
                    fread(&v, sizeof(md2_vertex), 1, file);
                    curvert->pos = vec(v.vertex[0]*frame.scale[0]+frame.translate[0],
                                       -(v.vertex[1]*frame.scale[1]+frame.translate[1]),
                                       v.vertex[2]*frame.scale[2]+frame.translate[2]);
                    float *norm = md2normaltable[v.normalindex];
                    curvert->norm = vec(norm[0], -norm[1], norm[2]);
                    curvert++;
                };
                frame_offset += header.framesize;
            };
                 
            fclose(file);
           
            return loaded = true;
        };

        void getdefaultanim(animstate &as, int anim, int varseed, float speed)
        {
            //                      0              3              6   7   8   9   10        12  13
            //                      D    D    D    D    D    D    A   P   I   R,  E    L    J   GS  GI
            static int _frame[] = { 178, 184, 190, 183, 189, 197, 46, 54, 0,  40, 162, 162, 67, 7,  6 };
            static int _range[] = { 6,   6,   8,   1,   1,   1,   8,  4,  40, 6,  1,   1,   1,  18, 1 };
            static int animfr[] = { 2, 5, 7, 8, 6, 9, 6, 10, 11, 12, 12, 13, 14 };

            as.speed = speed;
            if((size_t)anim >= sizeof(animfr)/sizeof(animfr[0]))
            {
                as.frame = 0;
                as.range = 1;
                return;
            };
            int n = animfr[anim];
            if(anim==ANIM_DYING || anim==ANIM_DEAD) n -= varseed%3;
            as.frame = _frame[n];
            as.range = _range[n];
        };
    };

    void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl)
    {
        if(!loaded) return;

        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(yaw+180, 0, 0, 1);
        glRotatef(pitch, 0, -1, 0);
        parts[0]->render(anim, varseed, speed, basetime, d);
        glPopMatrix();

        if(vwepmdl)
        {
            ((md2 *)vwepmdl)->parts[0]->index = parts.length();
            vwepmdl->setskin();
            vwepmdl->setshader();
            vwepmdl->render(anim, varseed, speed, basetime, x, y, z, yaw, pitch, d, NULL);
        };
    };

    bool load()
    { 
        if(loaded) return true;
        md2part &mdl = *new md2part;
        parts.add(&mdl);
        mdl.model = this;
        mdl.index = 0;
        char *pname = parentdir(loadname);
        s_sprintfd(name1)("packages/models/%s/tris.md2", loadname);
        if(!mdl.load(path(name1)))
        {
            s_sprintf(name1)("packages/models/%s/tris.md2", pname);    // try md2 in parent folder (vert sharing)
            if(!mdl.load(path(name1))) { delete[] pname; return false; };
        };
        Texture *skin, *masks;
        loadskin(loadname, pname, skin, masks, this);
        loopv(mdl.meshes)
        {
            mdl.meshes[i]->skin  = skin;
            mdl.meshes[i]->masks = masks;
        };
        if(skin==crosshair) conoutf("could not load model skin for %s", name1);
        loadingmd2 = this;
        s_sprintfd(name3)("packages/models/%s/md2.cfg", loadname);
        if(!execfile(name3))
        {
            s_sprintf(name3)("packages/models/%s/md2.cfg", pname);
            execfile(name3);
        };
        delete[] pname;
        loadingmd2 = 0;
        loopv(parts) parts[i]->scaleverts(scale/4.0f, vec(translate.x, -translate.y, translate.z));
        return loaded = true;
    };
};

void md2anim(char *anim, int *frame, int *range, char *s)
{
    if(!loadingmd2 || loadingmd2->parts.empty()) { conoutf("not loading an md2"); return; };
    int num = findanim(anim);
    if(num<0) { conoutf("could not find animation %s", anim); return; };
    float speed = s[0] ? atof(s) : 100.0f;
    loadingmd2->parts.last()->setanim(num, *frame, *range, speed);
};

COMMAND(md2anim, "siis");

