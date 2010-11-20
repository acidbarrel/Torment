// creation of scoreboard

struct scoreboard : g3d_callback
{
    bool scoreson;
    vec menupos;
    int menustart;
    fpsclient &cl;
    
    scoreboard(fpsclient &_cl) : scoreson(false), cl(_cl)
    {
        CCOMMAND(scoreboard, showscores, "D", self->showscores(args!=NULL));
    };

    void showscores(bool on)
    {
        if(!scoreson && on)
        {
            menupos = menuinfrontofplayer();
            menustart = cl.lastmillis;
        };
        scoreson = on;
    };

    struct sline { string s; };

    struct teamscore
    {
        char *team;
        int score;
        teamscore() {};
        teamscore(char *s, int n) : team(s), score(n) {};
    };

    static int teamscorecmp(const teamscore *x, const teamscore *y)
    {
        if(x->score > y->score) return -1;
        if(x->score < y->score) return 1;
        return 0;
    };
    
    static int playersort(const fpsent **a, const fpsent **b)
    {
        return (int)((*a)->frags<(*b)->frags)*2-1;
    };

    void gui(g3d_gui &g, bool firstpass)
    {
        g.start(menustart, 0.04f, NULL, false);
        
        g.text("frags\tpj\tping\tteam\tname", 0xFFFF80, "server");

        vector<teamscore> teamscores;
        bool showclientnum = cl.cc.currentmaster>=0 && cl.cc.currentmaster==cl.player1->clientnum;
        int gamemode = cl.gamemode;
        
        vector<fpsent *> sbplayers;

        loopi(cl.numdynents()) 
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && o->type!=ENT_AI) sbplayers.add(o);
        };
        
        sbplayers.sort(playersort);
        
        loopv(sbplayers) 
        {
            fpsent *o = sbplayers[i];
            const char *master = cl.cc.currentmaster>= 0 && (cl.cc.currentmaster==o->clientnum) ? "\f0" : "";
            string name;
            if(showclientnum) s_sprintf(name)("%s \f0(%d)", o->name, o->clientnum);
            else s_strcpy(name, o->name);
            string line;
            if(o->state == CS_SPECTATOR) s_sprintf(line)("SPECTATOR\t\t\t%s%s", master, name);
            else
            {
                s_sprintfd(lag)("%d", o->plag);
                s_sprintf(line)("%d\t%s\t%d\t%s\t%s%s", m_capture ? cl.cpc.findscore(o->team).total : o->frags, o->state==CS_LAGGED ? "LAG" : lag, o->ping, o->team, master, name);
            };
            g.text(line, 0xFFFFDD, "ogro");
        };

        if(m_teammode)
        {
            if(m_capture)
            {
                loopv(cl.cpc.scores) if(cl.cpc.scores[i].total)
                    teamscores.add(teamscore(cl.cpc.scores[i].team, cl.cpc.scores[i].total));
            }
            else loopi(cl.numdynents()) 
            {
                fpsent *o = (fpsent *)cl.iterdynents(i);
                if(o && o->type!=ENT_AI && o->frags)
                {
                    teamscore *ts = NULL;
                    loopv(teamscores) if(!strcmp(teamscores[i].team, o->team)) { ts = &teamscores[i]; break; };
                    if(!ts) teamscores.add(teamscore(o->team, o->frags));
                    else ts->score += o->frags;
                };
            };
            teamscores.sort(teamscorecmp);
            while(teamscores.length() && teamscores.last().score <= 0) teamscores.drop();
            if(teamscores.length())
            {
                string teamline;
                teamline[0] = 0;
                loopvj(teamscores)
                {
                    if(j >= 4) break;
                    s_sprintfd(s)("[ %s: %d ]", teamscores[j].team, teamscores[j].score);
                    s_strcat(teamline, s);
                };
                g.text(teamline, 0xFFFF40);
            };
        };
        
        g.end();
    };
    
    void show()
    {
        if(scoreson) g3d_addgui(this, menupos);
    };
};
