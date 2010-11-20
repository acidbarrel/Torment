
struct fpsrender
{      
    void rendergame(fpsclient &cl, int gamemode)
    {
//      const char *vweps[] = {NULL, "vwep/shotg", "vwep/chaing", "vwep/rocket", "vwep/rifle", "vwep/gl", "vwep/pistol"};
        const char *vweps[] = {NULL, "monster/ogro/vwep", "monster/ogro/vwep", "monster/ogro/vwep", "monster/ogro/vwep", "monster/ogro/vwep", "monster/ogro/vwep"};
        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR)
        {
            const char *mdlname = m_teammode ? (isteam(cl.player1->team, d->team) ? "monster/ogro/blue" : "monster/ogro/red") : (gamemode!=1 ? "monster/ogro/red" : "monster/ogro");
			//TOTAL MOD
			const char *vwepname = d->gunselect<=GUN_PISTOL ? vweps[d->gunselect] : NULL;
            if(d->state!=CS_DEAD || d->superdamage<50) renderclient(d, mdlname, vwepname, false, d->lastaction, d->lastpain);
            s_strcpy(d->info, d->name);
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); };
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, m_teammode ? (isteam(cl.player1->team, d->team) ? 16 : 13) : 11, 1);
        };
        if(isthirdperson()) renderclient(cl.player1, "monster/ogro", cl.player1->gunselect<=GUN_PISTOL ? vweps[cl.player1->gunselect] : NULL, false, cl.player1->lastaction, cl.player1->lastpain);

        cl.ms.monsterrender();
        cl.et.renderentities();
        cl.ws.renderprojectiles();
        if(m_capture) cl.cpc.renderbases();
    };

};
