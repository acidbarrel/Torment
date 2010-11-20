struct vertmodel : model
{
    struct anpos
    {
        int fr1, fr2;
        float t;
                
        void setframes(const animstate &as)
        {
            int time = lastmillis-as.basetime;
            fr1 = (int)(time/as.speed); // round to full frames
            t = (time-fr1*as.speed)/as.speed; // progress of the frame, value from 0.0f to 1.0f
            if(as.anim&ANIM_LOOP)
            {
                fr1 = fr1%as.range+as.frame;
                fr2 = fr1+1;
                if(fr2>=as.frame+as.range) fr2 = as.frame;
            }
            else
            {
                fr1 = min(fr1, as.range-1)+as.frame;
                fr2 = min(fr1+1, as.frame+as.range-1);
            };
            if(as.anim&ANIM_REVERSE)
            {
                fr1 = (as.frame+as.range-1)-(fr1-as.frame);
                fr2 = (as.frame+as.range-1)-(fr2-as.frame);
            };
        };
    };

    struct vert { vec norm, pos; };
    struct vvert : vert { float u, v; };
    struct tcvert { float u, v; ushort index; };
    struct tri { ushort vert[3]; };

    struct mesh
    {
        char *name;
        vert *verts;
        tcvert *tcverts;
        tri *tris;
        int numverts, numtcverts, numtris;

        Texture *skin, *masks;
        int tex;

        vert *dynbuf;
        ushort *dynidx;
        int dynframe, dynlen;
        GLuint statbuf, statidx;
        int statlen;

        mesh() : name(0), verts(0), tcverts(0), tris(0), skin(crosshair), masks(crosshair), tex(0), dynbuf(0), dynidx(0), dynframe(-1), statbuf(0), statidx(0) {};

        ~mesh()
        {
            DELETEA(name);
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(tris);
            if(hasVBO)
            {
                if(statbuf) glDeleteBuffers_(1, &statbuf);
                if(statidx) glDeleteBuffers_(1, &statidx);
            };
            DELETEA(dynidx);
            DELETEA(dynbuf);
        };

        void genva(bool dyn) // generate vbo's for each mesh
        {
            vector<ushort> idxs;
            vector<vvert> vverts;

            loopl(numtris)
            {
                tri &t = tris[l];
                if(dyn) loopk(3) idxs.add(t.vert[k]);
                else loopk(3)
                {
                    tcvert &tc = tcverts[t.vert[k]];
                    vert &v = verts[tc.index];
                    loopvj(vverts) // check if it's already added
                    {
                        vvert &w = vverts[j];
                        if(tc.u==w.u && tc.v==w.v && v.pos==w.pos && v.norm==w.norm) { idxs.add((ushort)j); goto found; };
                    };
                    {
                        idxs.add(vverts.length());
                        vvert &w = vverts.add();
                        w.pos = v.pos;
                        w.norm = v.norm;
                        w.u = tc.u;
                        w.v = tc.v;
                    }
                    found:;
                };
            };

            if(dyn)
            {
                tristrip ts;
                ts.addtriangles(idxs.getbuf(), idxs.length()/3);
                idxs.setsizenodelete(0);
                ts.buildstrips(idxs);
                dynbuf = new vert[numverts];
                dynidx = new ushort[idxs.length()];
                memcpy(dynidx, idxs.getbuf(), idxs.length()*sizeof(ushort));
                dynlen = idxs.length();
            }
            else
            {
                glGenBuffers_(1, &statbuf);
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, statbuf);
                glBufferData_(GL_ARRAY_BUFFER_ARB, vverts.length()*sizeof(vvert), vverts.getbuf(), GL_STATIC_DRAW_ARB);

                glGenBuffers_(1, &statidx);
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, statidx);
                glBufferData_(GL_ELEMENT_ARRAY_BUFFER_ARB, idxs.length()*sizeof(ushort), idxs.getbuf(), GL_STATIC_DRAW_ARB);
                statlen = idxs.length();
            };
        };

        void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12])
        {
            vert *fverts = &verts[frame*numverts];
            loopj(numverts)
            {
                vec &v = fverts[j].pos; 
                loopi(3)
                {
                    float c = m[i]*v.x + m[i+3]*v.y + m[i+6]*v.z + m[i+9];
                    bbmin[i] = min(bbmin[i], c);
                    bbmax[i] = max(bbmax[i], c);
                };
            };
        };

        void gentris(int frame, vector<triangle> &out, float m[12])
        {
            vert *fverts = &verts[frame*numverts];
            loopj(numtris)
            {
                triangle &t = out.add();
                vec &a = fverts[tcverts[tris[j].vert[0]].index].pos,
                    &b = fverts[tcverts[tris[j].vert[1]].index].pos,
                    &c = fverts[tcverts[tris[j].vert[2]].index].pos;
                loopi(3)
                {
                    t.a[i] = m[i]*a.x + m[i+3]*a.y + m[i+6]*a.z + m[i+9];
                    t.b[i] = m[i]*b.x + m[i+3]*b.y + m[i+6]*b.z + m[i+9];
                    t.c[i] = m[i]*c.x + m[i+3]*c.y + m[i+6]*c.z + m[i+9];
                };
            };
        };

        void gendynverts(anpos &cur, anpos *prev, float ai_t)
        {
            vert *vert1 = &verts[cur.fr1 * numverts],
                 *vert2 = &verts[cur.fr2 * numverts],
                 *pvert1 = NULL, *pvert2 = NULL;
            if(prev)
            {
                pvert1 = &verts[prev->fr1 * numverts];
                pvert2 = &verts[prev->fr2 * numverts];
                dynframe = -1;
            }
            else if(cur.fr1==cur.fr2)
            {
                if(cur.fr1==dynframe) return;
                dynframe = cur.fr1;
            }
            else dynframe = -1;
            loopi(numverts) // vertices
            {
                vert &v = dynbuf[i];
                #define ip(p1, p2, t) (p1+t*(p2-p1))
                #define ip_v(p, c, t) ip(p##1[i].c, p##2[i].c, t)
                if(prev)
                {
                    #define ip_v_ai(c) ip( ip_v(pvert, c, prev->t), ip_v(vert, c, cur.t), ai_t)
                    v.norm = vec(ip_v_ai(norm.x), ip_v_ai(norm.y), ip_v_ai(norm.z));
                    v.pos = vec(ip_v_ai(pos.x), ip_v_ai(pos.y), ip_v_ai(pos.z));
                    #undef ip_v_ai
                }
                else
                {
                    v.norm = vec(ip_v(vert, norm.x, cur.t), ip_v(vert, norm.y, cur.t), ip_v(vert, norm.z, cur.t));
                    v.pos = vec(ip_v(vert, pos.x, cur.t), ip_v(vert, pos.y, cur.t), ip_v(vert, pos.z, cur.t));
                };
                #undef ip
                #undef ip_v
            };
        };

        void bindskin()
        {
            Texture *s = skin, *m = masks;
            if(tex)
            {
                Slot &slot = lookuptexture(tex);
                s = slot.sts[0].t;
                m = slot.sts.length() >= 2 ? slot.sts[1].t : crosshair;
            };
            if(s->bpp==32) // transparent ss
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glEnable(GL_ALPHA_TEST);
                glAlphaFunc(GL_GREATER, 0.9f);
            };
            glBindTexture(GL_TEXTURE_2D, s->gl);
            if(m!=crosshair)
            {
                glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, m->gl);
                glActiveTexture_(GL_TEXTURE0_ARB);
            };
        };

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t)
        {
            bindskin();

            if(hasVBO && as.frame==0 && as.range==1 && statbuf) // vbo's for static stuff
            {
                glBindBuffer_(GL_ARRAY_BUFFER_ARB, statbuf);
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, statidx);
                glEnableClientState(GL_VERTEX_ARRAY);
                vvert *vverts = 0;
                glVertexPointer(3, GL_FLOAT, sizeof(vvert), &vverts->pos);
                glEnableClientState(GL_NORMAL_ARRAY);
                glNormalPointer(GL_FLOAT, sizeof(vvert), &vverts->norm);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, sizeof(vvert), &vverts->u);

                glDrawElements(GL_TRIANGLES, statlen, GL_UNSIGNED_SHORT, 0);

                glBindBuffer_(GL_ARRAY_BUFFER_ARB, 0);
                glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glDisableClientState(GL_NORMAL_ARRAY);
                glDisableClientState(GL_VERTEX_ARRAY);

                xtravertsva += numtcverts;
            }
            else if(dynbuf)
            {
                gendynverts(cur, prev, ai_t);
                loopj(dynlen)
                {
                    ushort index = dynidx[j];
                    if(index>=tristrip::RESTART || !j)
                    {
                        if(j) glEnd();
                        glBegin(index==tristrip::LIST ? GL_TRIANGLES : GL_TRIANGLE_STRIP);
                        if(index>=tristrip::RESTART) continue;
                    };
                    tcvert &tc = tcverts[index];
                    vert &v = dynbuf[tc.index];
                    glTexCoord2f(tc.u, tc.v);
                    glNormal3fv(v.norm.v);
                    glVertex3fv(v.pos.v);
                };
                glEnd();
                xtraverts += dynlen;
            };

            if(skin && skin->bpp==32)
            {
                glDisable(GL_ALPHA_TEST);
                glDisable(GL_BLEND);
            };
        };                     
    };

    struct animinfo
    {
        int frame, range;
        float speed;
    };

    struct tag
    {
        char *name;
        vec pos;
        float transform[3][3];
        
        tag() : name(NULL) {};
        ~tag() { DELETEA(name); };
    };

    struct part
    {
        bool loaded;
        vertmodel *model;
        int index, numframes;
        vector<mesh *> meshes;
        vector<animinfo> *anims;
        part **links;
        tag *tags;
        int numtags;

        part() : loaded(false), anims(NULL), links(NULL), tags(NULL), numtags(0) {};
        virtual ~part()
        {
            meshes.deletecontentsp();
            DELETEA(anims);
            DELETEA(links);
            DELETEA(tags);
        };

        bool link(part *link, const char *tag)
        {
            loopi(numtags) if(!strcmp(tags[i].name, tag))
            {
                links[i] = link;
                return true;
            };
            return false;
        };

        void scaleverts(const float scale, const vec &translate)
        {
           loopv(meshes)
           {
               mesh &m = *meshes[i];
               loopj(numframes*m.numverts)
               {
                   vec &v = m.verts[j].pos;
                   if(!index) v.add(translate);
                   v.mul(scale);
               };
           };
           loopi(numframes*numtags)
           {
               vec &v = tags[i].pos;
               if(!index) v.add(translate);
               v.mul(scale);
           };
        };

        void genva(bool dyn) // generate vbo's for each mesh
        {
            loopv(meshes) meshes[i]->genva(dyn);
        };
            
        void calctransform(tag &t, float m[12], float n[12])
        {
            loop(y, 3)
            {
                n[y] = m[y]*t.transform[0][0] + m[y+3]*t.transform[0][1] + m[y+6]*t.transform[0][2];
                n[3+y] = m[y]*t.transform[1][0] + m[y+3]*t.transform[1][1] + m[y+6]*t.transform[1][2];
                n[6+y] = m[y]*t.transform[2][0] + m[y+3]*t.transform[2][1] + m[y+6]*t.transform[2][2];
                n[9+y] = m[y]*t.pos[0] + m[y+3]*t.pos[1] + m[y+6]*t.pos[2] + m[y+9];
            };
        };

        void calcbb(int frame, vec &bbmin, vec &bbmax)
        {
            float m[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            calcbb(frame, bbmin, bbmax, m);
        };

        void calcbb(int frame, vec &bbmin, vec &bbmax, float m[12])
        {
            loopv(meshes) meshes[i]->calcbb(frame, bbmin, bbmax, m);
            loopi(numtags) if(links[i])
            {
                tag &t = tags[frame*numtags+i];
                float n[12];
                calctransform(t, m, n);
                links[i]->calcbb(frame, bbmin, bbmax, n);
            };
        };

        void gentris(int frame, vector<triangle> &tris)
        {
            float m[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
            gentris(frame, tris, m);
        };

        void gentris(int frame, vector<triangle> &tris, float m[12])
        {
            loopv(meshes) meshes[i]->gentris(frame, tris, m);
            loopi(numtags) if(links[i])
            {
                tag &t = tags[frame*numtags+i];
                float n[12];
                calctransform(t, m, n);
                links[i]->gentris(frame, tris, n);
            };
        };

        virtual void getdefaultanim(animstate &as, int anim, int varseed, float speed)
        {
            as.frame = 0;
            as.range = 1;
            as.speed = speed;
        };

        bool calcanimstate(int anim, int varseed, float speed, int basetime, dynent *d, animstate &as)
        {
            as.anim = anim;
            as.basetime = basetime;
            if(anims)
            {
                vector<animinfo> &ais = anims[anim&ANIM_INDEX];
                if(ais.length())
                {
                    animinfo &ai = ais[varseed%ais.length()];
                    as.frame = ai.frame;
                    as.range = ai.range;
                    as.speed = speed*100.0f/ai.speed;
                }
                else
                {
                    as.frame = 0;
                    as.range = 1;
                    as.speed = speed;
                };
            }
            else getdefaultanim(as, anim&ANIM_INDEX, varseed, speed);
            if(anim&(ANIM_START|ANIM_END))
            {
                if(anim&ANIM_END) as.frame += as.range-1;
                as.range = 1; 
            };

            if(as.frame+as.range>numframes)
            {
                if(as.frame>=numframes) return false;
                as.range = numframes-as.frame;
            };

            if(d && index<2)
            {
                if(d->lastmodel[index]!=this || d->lastanimswitchtime[index]==-1)
                {
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis-animationinterpolationtime*2;
                }
                else if(d->current[index] != as)
                {
                    if(lastmillis-d->lastanimswitchtime[index]>animationinterpolationtime/2) d->prev[index] = d->current[index];
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis;
                };
                d->lastmodel[index] = this;
            };
            return true;
        };
        
        void render(int anim, int varseed, float speed, int basetime, dynent *d)
        {
            if(meshes.length() <= 0) return;
            animstate as;
            if(!calcanimstate(anim, varseed, speed, basetime, d, as)) return;
    
            if(hasVBO && !meshes[0]->statbuf && as.frame==0 && as.range==1) genva(false);
            else if(!meshes[0]->dynbuf && (!hasVBO || as.frame!=0 || as.range!=1)) genva(true);
    
            anpos prev, cur;
            cur.setframes(d && index<2 ? d->current[index] : as);
    
            float ai_t = 0;
            bool doai = d && index<2 && lastmillis-d->lastanimswitchtime[index]<animationinterpolationtime;
            if(doai)
            {
                prev.setframes(d->prev[index]);
                ai_t = (lastmillis-d->lastanimswitchtime[index])/(float)animationinterpolationtime;
            };
            
            loopv(meshes) meshes[i]->render(as, cur, doai ? &prev : NULL, ai_t);

            loopi(numtags) if(links[i]) // render the linked models - interpolate rotation and position of the 'link-tags'
            {
                part *link = links[i];
                if(link->model!=model) link->model->setshader();

                GLfloat matrix[16];
                tag *tag1 = &tags[cur.fr1*numtags+i];
                tag *tag2 = &tags[cur.fr2*numtags+i];
                #define ip(p1, p2, t) (p1+t*(p2-p1))
                #define ip_ai_tag(c) ip( ip( tag1p->c, tag2p->c, prev.t), ip( tag1->c, tag2->c, cur.t), ai_t)
                if(doai)
                {
                    tag *tag1p = &tags[prev.fr1 * numtags + i];
                    tag *tag2p = &tags[prev.fr2 * numtags + i];
                    loopj(3) matrix[j] = ip_ai_tag(transform[0][j]); // transform
                    loopj(3) matrix[4 + j] = ip_ai_tag(transform[1][j]);
                    loopj(3) matrix[8 + j] = ip_ai_tag(transform[2][j]);
                    loopj(3) matrix[12 + j] = ip_ai_tag(pos[j]); // position      
                }
                else
                {
                    loopj(3) matrix[j] = ip(tag1->transform[0][j], tag2->transform[0][j], cur.t); // transform
                    loopj(3) matrix[4 + j] = ip(tag1->transform[1][j], tag2->transform[1][j], cur.t);
                    loopj(3) matrix[8 + j] = ip(tag1->transform[2][j], tag2->transform[2][j], cur.t);
                    loopj(3) matrix[12 + j] = ip(tag1->pos[j], tag2->pos[j], cur.t); // position
                };
                #undef ip_ai_tag
                #undef ip 
                matrix[3] = matrix[7] = matrix[11] = 0.0f;
                matrix[15] = 1.0f;
                glPushMatrix();
                    glMultMatrixf(matrix);
                    link->render(anim, varseed, speed, basetime, d);
                glPopMatrix();

                if(link->model!=model && i+1<numtags) model->setshader();
            };
        };

        void setanim(int num, int frame, int range, float speed)
        {
            if(frame<0 || frame>=numframes || range<=0 || frame+range>numframes) 
            { 
                conoutf("invalid frame %d, range %d in model %s", frame, range, model->loadname); 
                return; 
            };
            if(!anims) anims = new vector<animinfo>[NUMANIMS];
            animinfo &ai = anims[num].add();
            ai.frame = frame;
            ai.range = range;
            ai.speed = speed;
        };
    };

    bool loaded;
    char *loadname;
    vector<part *> parts;

    vertmodel(const char *name) : loaded(false)
    {
        loadname = newstring(name);
    };

    ~vertmodel()
    {
        delete[] loadname;
        parts.deletecontentsp();
    };

    char *name() { return loadname; };

    void gentris(int frame, vector<triangle> &tris)
    {
        loopv(parts) parts[i]->gentris(frame, tris);
    };

    SphereTree *setspheretree()
    {
        if(spheretree) return spheretree;
        vector<triangle> tris;
        gentris(0, tris);
        spheretree = buildspheretree(tris.length(), tris.getbuf());
        return spheretree;
    };

    bool link(part *link, const char *tag)
    {
        loopv(parts) if(parts[i]->link(link, tag)) return true;
        return false;
    };

    void setskin(int tex)
    {
        if(parts.length()!=1 || parts[0]->meshes.length()!=1) return;
        mesh &m = *parts[0]->meshes[0]; 
        m.tex = tex;
        masked = tex ? lookuptexture(tex).sts.length()>=2 : m.masks!=crosshair;
    };

    void calcbb(int frame, vec &center, vec &radius)
    {
        if(!loaded) return;
        vec bbmin(0, 0, 0), bbmax(0, 0, 0);
        loopv(parts[0]->meshes)
        {
            mesh &m = *parts[0]->meshes[i];
            if(m.numverts)
            {
                bbmin = bbmax = m.verts[frame*m.numverts].pos;
                break;
            };
        };
        parts[0]->calcbb(frame, bbmin, bbmax);
        radius = bbmax;
        radius.sub(bbmin);
        radius.mul(0.5f);
        center = bbmin;
        center.add(radius);
    };
};

