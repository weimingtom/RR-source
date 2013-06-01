#include "game.h"
#include "messageSystem.h"
#include "engine.h"

namespace game
{
    bool intermission = false;
    int maptime = 0, maprealtime = 0, maplimit = -1;
    int respawnent = -1;
    int lasthit = 0, lastspawnattempt = 0;

    int following = -1, followdir = 0;

    fpsent *player1 = NULL;         // our client
    vector<fpsent *> players;       // other clients
    int savedammo[NUMGUNS];

    bool clientoption(const char *arg) { return false; }

    void taunt()
    {
        if(player1->state!=CS_ALIVE || player1->physstate<PHYS_SLOPE) return;
        if(lastmillis-player1->lasttaunt<1000) return;
        player1->lasttaunt = lastmillis;
        addmsg(N_TAUNT, "rc", player1);
    }
    COMMAND(taunt, "");

    ICOMMAND(getfollow, "", (),
    {
        fpsent *f = followingplayer();
        intret(f ? f->clientnum : -1);
    });

	void follow(char *arg)
    {
        if(arg[0] ? player1->state==CS_SPECTATOR : following>=0)
        {
            following = arg[0] ? parseplayer(arg) : -1;
            if(following==player1->clientnum) following = -1;
            followdir = 0;
            conoutf("follow %s", following>=0 ? "on" : "off");
        }
	}
    COMMAND(follow, "s");

    void nextfollow(int dir)
    {
        if(player1->state!=CS_SPECTATOR || clients.empty())
        {
            stopfollowing();
            return;
        }
        int cur = following >= 0 ? following : (dir < 0 ? clients.length() - 1 : 0);
        loopv(clients)
        {
            cur = (cur + dir + clients.length()) % clients.length();
            if(clients[cur] && clients[cur]->state!=CS_SPECTATOR)
            {
                if(following<0) conoutf("follow on");
                following = cur;
                followdir = dir;
                return;
            }
        }
        stopfollowing();
    }
    ICOMMAND(nextfollow, "i", (int *dir), nextfollow(*dir < 0 ? -1 : 1));


    const char *getclientmap() { return clientmap; }

    void resetgamestate()
    {
        clearprojectiles();
        clearbouncers();
    }

    fpsent *spawnstate(fpsent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate(gamemode);
        return d;
    }
    void respawnself()
    {
        if(ispaused()) return;
		if(player1->pclass <0){showgui("playerclass");return;}
        if(m_mp(gamemode))
        {
            int seq = (player1->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
            if(player1->respawned!=seq) { addmsg(N_TRYSPAWN, "rc", player1); player1->respawned = seq; }
        }
        else
        {
            spawnplayer(player1);
            showscores(false);
            lasthit = 0;
            if(cmode) cmode->respawned(player1);
        }
    }

    fpsent *pointatplayer()
    {
        loopv(players) if(players[i] != player1 && intersect(players[i], player1->o, worldpos)) return players[i];
        return NULL;
    }

    void stopfollowing()
    {
        if(following<0) return;
        following = -1;
        followdir = 0;
        conoutf("follow off");
    }

    fpsent *followingplayer()
    {
        if(player1->state!=CS_SPECTATOR || following<0) return NULL;
        fpsent *target = getclient(following);
        if(target && target->state!=CS_SPECTATOR) return target;
        return NULL;
    }

    fpsent *hudplayer()
    {
        if(thirdperson) return player1;
        fpsent *target = followingplayer();
        return target ? target : player1;
    }

    void setupcamera()
    {
        fpsent *target = followingplayer();
        if(target)
        {
            player1->yaw = target->yaw;
            player1->pitch = target->state==CS_DEAD ? 0 : target->pitch;
            player1->o = target->o;
            player1->resetinterp();
        }
    }

    bool detachcamera()
    {
        fpsent *d = hudplayer();
        return d->state==CS_DEAD;
    }

    bool collidecamera()
    {
        switch(player1->state)
        {
            case CS_EDITING: return false;
            case CS_SPECTATOR: return followingplayer()!=NULL;
        }
        return true;
    }

    VARP(smoothmove, 0, 75, 100);
    VARP(smoothdist, 0, 32, 64);

    void predictplayer(fpsent *d, bool move)
    {
        d->o = d->newpos;
        d->yaw = d->newyaw;
        d->pitch = d->newpitch;
        d->roll = d->newroll;
        if(move)
        {
            moveplayer(d, 1, false);
            d->newpos = d->o;
        }
        float k = 1.0f - float(lastmillis - d->smoothmillis)/smoothmove;
        if(k>0)
        {
            d->o.add(vec(d->deltapos).mul(k));
            d->yaw += d->deltayaw*k;
            if(d->yaw<0) d->yaw += 360;
            else if(d->yaw>=360) d->yaw -= 360;
            d->pitch += d->deltapitch*k;
            d->roll += d->deltaroll*k;
        }
    }

    void otherplayers(int curtime)
    {
        loopv(players)
        {
            fpsent *d = players[i];
            if(d == player1 || d->ai) continue;

            if(d->state==CS_DEAD && d->ragdoll) moveragdoll(d);
            else if(!intermission)
            {
                if(lastmillis - d->lastaction >= d->gunwait) d->gunwait = 0;
                if(d->quadmillis) entities::checkquad(curtime, d);
            }

            const int lagtime = totalmillis-d->lastupdate;
            if(!lagtime || intermission) continue;
            else if(lagtime>1000 && d->state==CS_ALIVE)
            {
                d->state = CS_LAGGED;
                continue;
            }
            if(d->state==CS_ALIVE || d->state==CS_EDITING)
            {
                crouchplayer(d, 10, false);
                if(smoothmove && d->smoothmillis>0) predictplayer(d, true);
                else moveplayer(d, 1, false);
            }
            else if(d->state==CS_DEAD && !d->ragdoll && lastmillis-d->lastpain<2000) moveplayer(d, 1, true);
        }
    }

    void updateworld()        // main game update loop
    {
        if(!maptime) { maptime = lastmillis; maprealtime = totalmillis; return; }
        if(!curtime) { gets2c(); if(player1->clientnum>=0) c2sinfo(); return; }

        physicsframe();
        ai::navigate();
        if(player1->state != CS_DEAD && !intermission)
        {
            if(player1->quadmillis) entities::checkquad(curtime, player1);
        }
        updateweapons(curtime);
        otherplayers(curtime);
        ai::update();
        moveragdolls();
        gets2c();
        if(player1->state == CS_DEAD)
        {
            if(player1->ragdoll) moveragdoll(player1);
            else if(lastmillis-player1->lastpain<2000)
            {
                player1->move = player1->strafe = 0;
                moveplayer(player1, 10, true);
            }
        }
        else if(!intermission)
        {
            if(player1->ragdoll) cleanragdoll(player1);
            crouchplayer(player1, 10, true);
            moveplayer(player1, 10, true);
            swayhudgun(curtime);
            entities::checkitems(player1);
            if(cmode) cmode->checkitems(player1);
        }
        if(player1->clientnum>=0) c2sinfo();   // do this last, to reduce the effective frame lag
    }

    VARP(spawnwait, 0, 0, 1000);
	 void respawn()
    {
        if(player1->state==CS_DEAD)
        {
            player1->attacking = false;
            int wait = cmode ? cmode->respawnwait(player1) : 0;
            if(wait>0)
            {
                lastspawnattempt = lastmillis;
                //conoutf(CON_GAMEINFO, "\f2you must wait %d second%s before respawn!", wait, wait!=1 ? "s" : "");
                return;
            }
            if(lastmillis < player1->lastpain + spawnwait) return;
            respawnself();
        }
    }
	void switchplayerclass(int playerclass){player1->pclass = playerclass;};

	VARFP(playerclass, 0, 0, NUMPCS-1, if(player1->pclass != playerclass) switchplayerclass(playerclass) );

    void spawnplayer(fpsent *d)   // place at random spawn
    {
        if(cmode) cmode->pickspawn(d);
        else findplayerspawn(d, d==player1 && respawnent>=0 ? respawnent : -1);
        spawnstate(d);
        if(d==player1)
        {
            if(editmode) d->state = CS_EDITING;
            else if(d->state != CS_SPECTATOR) d->state = CS_ALIVE;
			//else if(playerclass == -1){showgui("playerclass");}
        }
        else d->state = CS_ALIVE;
    }


    // inputs

    void doattack(bool on)
    {
        if(intermission) return;
        if((player1->attacking = on)) respawn();
    }

    bool canjump()
    {
        if(!intermission) respawn();
        return player1->state!=CS_DEAD && !intermission;
    }

    bool cancrouch()
    {
        return player1->state!=CS_DEAD && !intermission;
    }

    bool allowmove(physent *d)
    {
        if(d->type!=ENT_PLAYER) return true;
        return !((fpsent *)d)->lasttaunt || lastmillis-((fpsent *)d)->lasttaunt>=1000;
    }

    VARP(hitsound, 0, 0, 1);

    void damaged(int damage, fpsent *d, fpsent *actor, bool local)
    {
        if((d->state!=CS_ALIVE && d->state != CS_LAGGED && d->state != CS_SPAWNING) || intermission) return;

        if(local) damage = d->dodamage(damage);
        else if(actor==player1) return;

        fpsent *h = hudplayer();
        if(h!=player1 && actor==h && d!=actor)
        {
            if(hitsound && lasthit != lastmillis) playsound(S_HIT);
            lasthit = lastmillis;
        }
        if(d==h)
        {
            damageblend(damage);
            damagecompass(damage, actor->o);
        }
        damageeffect(damage, d, d!=h);

		ai::damaged(d, actor);

        if(d->health<=0) { if(local) killed(d, actor); }
        else if(d==h) playsound(S_PAIN6);
        else playsound(S_PAIN1+rnd(5), &d->o);
    }

    VARP(deathscore, 0, 1, 1);

    void deathstate(fpsent *d, bool restore)
    {
        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        if(!restore) gibeffect(max(-d->health, 0), d->vel, d);
        if(d==player1)
        {
            if(deathscore) showscores(true);
            disablezoom();
            if(!restore) loopi(NUMGUNS) savedammo[i] = player1->ammo[i];
            d->attacking = false;
            if(!restore) d->deaths++;
            //d->pitch = 0;
            d->roll = 0;
            playsound(S_DIE1+rnd(2));
        }
        else
        {
            d->move = d->strafe = 0;
            d->resetinterp();
            d->smoothmillis = 0;
            playsound(S_DIE1+rnd(2), &d->o);
        }
    }

    VARP(teamcolorfrags, 0, 1, 1);

    void killed(fpsent *d, fpsent *actor)
    {
        if(d->state==CS_EDITING)
        {
            d->editstate = CS_DEAD;
            if(d==player1) d->deaths++;
            else d->resetinterp();
            return;
        }
        else if((d->state!=CS_ALIVE && d->state != CS_LAGGED && d->state != CS_SPAWNING) || intermission) return;

        fpsent *h = followingplayer();
        if(!h) h = player1;
        int contype = d==h || actor==h ? CON_FRAG_SELF : CON_FRAG_OTHER;
        const char *dname = "", *aname = "";
        if(teamcolorfrags)
        {
            dname = teamcolorname(d, "you");
            aname = teamcolorname(actor, "you");
        }
        else
        {
            dname = colorname(d, NULL, "", "", "you");
            aname = colorname(actor, NULL, "", "", "you");
        }
        if(d==actor)
            conoutf(contype, "\f2%s suicided%s", dname, d==player1 ? "!" : "");
        else if(isteam(d->team, actor->team))
        {
            contype |= CON_TEAMKILL;
            if(actor==player1) conoutf(contype, "\f6%s fragged a teammate (%s)", aname, dname);
            else if(d==player1) conoutf(contype, "\f6%s got fragged by a teammate (%s)", dname, aname);
            else conoutf(contype, "\f2%s fragged a teammate (%s)", aname, dname);
        }
        else
        {
            if(d==player1) conoutf(contype, "\f2%s got fragged by %s", dname, aname);
            else conoutf(contype, "\f2%s fragged %s", aname, dname);
        }
        deathstate(d);
		ai::killed(d, actor);
    }

    void timeupdate(int secs)
    {
        if(secs > 0)
        {
            maplimit = lastmillis + secs*1000;
        }
        else
        {
            intermission = true;
            player1->attacking = false;
            if(cmode) cmode->gameover();
            conoutf(CON_GAMEINFO, "\f2intermission:");
            conoutf(CON_GAMEINFO, "\f2game has ended!");
            if(m_ctf) conoutf(CON_GAMEINFO, "\f2player frags: %d, flags: %d, deaths: %d", player1->frags, player1->flags, player1->deaths);
            else if(m_collect) conoutf(CON_GAMEINFO, "\f2player frags: %d, skulls: %d, deaths: %d", player1->frags, player1->flags, player1->deaths);
            else conoutf(CON_GAMEINFO, "\f2player frags: %d, deaths: %d", player1->frags, player1->deaths);
            int accuracy = (player1->totaldamage*100)/max(player1->totalshots, 1);
            conoutf(CON_GAMEINFO, "\f2player total damage dealt: %d, damage wasted: %d, accuracy(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);

            showscores(true);
            disablezoom();

            if(identexists("intermission")) execute("intermission");
        }
    }

    ICOMMAND(getfrags, "", (), intret(player1->frags));
    ICOMMAND(getflags, "", (), intret(player1->flags));
    ICOMMAND(getdeaths, "", (), intret(player1->deaths));
    ICOMMAND(getaccuracy, "", (), intret((player1->totaldamage*100)/max(player1->totalshots, 1)));
    ICOMMAND(gettotaldamage, "", (), intret(player1->totaldamage));
    ICOMMAND(gettotalshots, "", (), intret(player1->totalshots));

    vector<fpsent *> clients;

    fpsent *newclient(int cn)   // ensure valid entity
    {
        if(cn < 0 || cn > max(0xFF, MAXCLIENTS + MAXBOTS))
        {
            neterr("clientnum", false);
            return NULL;
        }

        if(cn == player1->clientnum) return player1;

        while(cn >= clients.length()) clients.add(NULL);
        if(!clients[cn])
        {
            fpsent *d = new fpsent;
            d->clientnum = cn;
            clients[cn] = d;
            players.add(d);
        }
        return clients[cn];
    }

    fpsent *getclient(int cn)   // ensure valid entity
    {
        if(cn == player1->clientnum) return player1;
        return clients.inrange(cn) ? clients[cn] : NULL;
    }

    void clientdisconnected(int cn, bool notify)
    {
        if(!clients.inrange(cn)) return;
        if(following==cn)
        {
            if(followdir) nextfollow(followdir);
            else stopfollowing();
        }
        unignore(cn);
        fpsent *d = clients[cn];
        if(!d) return;
        if(notify && d->name[0]) conoutf("\f4leave:\f7 %s", colorname(d));
        removeweapons(d);
        removetrackedparticles(d);
        removetrackeddynlights(d);
        if(cmode) cmode->removeplayer(d);
        players.removeobj(d);
        DELETEP(clients[cn]);
        cleardynentcache();
    }

    void clearclients(bool notify)
    {
        loopv(clients) if(clients[i]) clientdisconnected(i, notify);
    }

    void initclient()
    {
        LOG_INFO("Initializing client");
        player1 = spawnstate(new fpsent);
        players.add(player1);
        messageSystem::MessageManager::registerInternal();
    }

    VARP(showmodeinfo, 0, 1, 1);


    void startgame()
    {
        clearprojectiles();
        clearbouncers();
        clearragdolls();

        clearteaminfo();

        // reset perma-state
        loopv(players)
        {
            fpsent *d = players[i];
            d->frags = d->flags = 0;
            d->deaths = 0;
            d->totaldamage = 0;
            d->totalshots = 0;
            d->maxhealth = 100;
            d->lifesequence = -1;
            d->respawned = d->suicided = -2;
        }

        setclientmode();

        intermission = false;
        maptime = maprealtime = 0;
        maplimit = -1;

        if(cmode)
        {
            cmode->preload();
            cmode->setup();
        }

        conoutf(CON_GAMEINFO, "\f2game mode is %s", server::modename(gamemode));

        const char *info = m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
        if(showmodeinfo && info) conoutf(CON_GAMEINFO, "\f0%s", info);

        if(player1->playermodel != playermodel) switchplayermodel(playermodel);

        showscores(false);
        disablezoom();
        lasthit = 0;

        if(identexists("mapstart")) execute("mapstart");
    }

    void startmap(const char *name)   // called just after a map load
    {
        ai::savewaypoints();
        ai::clearwaypoints(true);

        respawnent = -1; // so we don't respawn at an old spot
        if(!m_mp(gamemode)) spawnplayer(player1);
        else findplayerspawn(player1, -1);
        entities::resetspawns();
        copystring(clientmap, name ? name : "");

        sendmapinfo();
    }

    const char *getmapinfo()
    {
        return showmodeinfo && m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
    }

    const char *getscreenshotinfo()
    {
        return server::modename(gamemode, NULL);
    }

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
    {
        if     (waterlevel>0) { if(material!=MAT_LAVA) playsound(S_SPLASH1, d==player1 ? NULL : &d->o); }
        else if(waterlevel<0) playsound(material==MAT_LAVA ? S_BURN : S_SPLASH2, d==player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(d==player1 || d->type!=ENT_PLAYER || ((fpsent *)d)->ai) msgsound(S_JUMP, d); }
        else if(floorlevel<0) { if(d==player1 || d->type!=ENT_PLAYER || ((fpsent *)d)->ai) msgsound(S_LAND, d); }
    }

    void dynentcollide(physent *d, physent *o, const vec &dir)
    {
    }

    void msgsound(int n, physent *d)
    {
        if(!d || d==player1)
        {
            addmsg(N_SOUND, "ci", d, n);
            playsound(n);
        }
        else
        {
            if(d->type==ENT_PLAYER && ((fpsent *)d)->ai)
                addmsg(N_SOUND, "ci", d, n);
            playsound(n, &d->o);
        }
    }

    int numdynents() { return players.length(); }

    dynent *iterdynents(int i)
    {
        if(i<players.length()) return players[i];
        return NULL;
    }

    bool duplicatename(fpsent *d, const char *name = NULL, const char *alt = NULL)
    {
        if(!name) name = d->name;
        if(alt && d != player1 && !strcmp(name, alt)) return true;
        loopv(players) if(d!=players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    static string cname[3];
    static int cidx = 0;

    const char *colorname(fpsent *d, const char *name, const char *prefix, const char *suffix, const char *alt)
    {
        if(!name) name = alt && d == player1 ? alt : d->name;
        bool dup = !name[0] || duplicatename(d, name, alt) || d->aitype != AI_NONE;
        if(dup || prefix[0] || suffix[0])
        {
            cidx = (cidx+1)%3;
            if(dup) formatstring(cname[cidx])(d->aitype == AI_NONE ? "%s%s \fs\f5(%d)\fr%s" : "%s%s \fs\f5[%d]\fr%s", prefix, name, d->clientnum, suffix);
            else formatstring(cname[cidx])("%s%s%s", prefix, name, suffix);
            return cname[cidx];
        }
        return name;
    }

    VARP(teamcolortext, 0, 1, 1);

    const char *teamcolorname(fpsent *d, const char *alt)
    {
        if(!teamcolortext) return colorname(d, NULL, "", "", alt);
        return colorname(d, NULL, isteam(d->team, player1->team) ? "\fs\f1" : "\fs\f3", "\fr", alt);
    }

    const char *teamcolor(const char *name, bool sameteam, const char *alt)
    {
        if(!teamcolortext) return sameteam || !alt ? name : alt;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx])(sameteam ? "\fs\f1%s\fr" : "\fs\f3%s\fr", sameteam || !alt ? name : alt);
        return cname[cidx];
    }

    const char *teamcolor(const char *name, const char *team, const char *alt)
    {
        return teamcolor(name, team && isteam(team, player1->team), alt);
    }

    void suicide(physent *d)
    {
        if(d==player1 || (d->type==ENT_PLAYER && ((fpsent *)d)->ai))
        {
            if(d->state!=CS_ALIVE) return;
            fpsent *pl = (fpsent *)d;
            if(!m_mp(gamemode)) killed(pl, pl);
            else
            {
                int seq = (pl->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
                if(pl->suicided!=seq) { addmsg(N_SUICIDE, "rc", pl); pl->suicided = seq; }
            }
        }
    }
    ICOMMAND(kill, "", (), suicide(player1));

	bool needminimap() { return true;}// m_ctf || m_capture; }


    void drawicon(int icon, float x, float y, float sz)
    {
        settexture("@{tig/rr-core}/texture/hud/items.png");
        float tsz = 0.25f, tx = tsz*(icon%4), ty = tsz*(icon/4);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,    y);    gle::attribf(tx,     ty);
        gle::attribf(x+sz, y);    gle::attribf(tx+tsz, ty);
        gle::attribf(x,    y+sz); gle::attribf(tx,     ty+tsz);
        gle::attribf(x+sz, y+sz); gle::attribf(tx+tsz, ty+tsz);
        gle::end();
    }

    float abovegameplayhud(int w, int h)
    {
        switch(hudplayer()->state)
        {
            case CS_EDITING:
            case CS_SPECTATOR:
                return 1;
            default:
                return 1650.0f/1800.0f;
        }
    }
        void drawometer(float progress, float size, vec pos, vec color)
        {
        pushhudmatrix();

//todo          glColor3fv(color.v);
        //hudmatrix.d = vec4(color.x, color.y, color.z, 1);
        hudmatrix.translate(vec(pos.x, pos.y, 0));
        hudmatrix.scale(size, size, 1);
        flushhudmatrix(); //needed?

                int angle = progress*360.f;
                int loops = angle/45;
                angle %= 45;
                float x = 0, y = -.5;
                settexture("@{tig/rr-core}/texture/hud/meter.png");
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_FAN);
                gle::attribf(.5, .5);   gle::attribf(0, 0);
                gle::attribf(.5, 0);    gle::attribf(x, y);
                loopi(loops)
                {
                        y==0||y==x? y -= x: x += y; // y-x==0
                        gle::attribf(x+.5, y+.5);       gle::attribf(x, y);
                }

                if (angle)
                {
                        float ang = sinf(angle*RAD)*(sqrtf(powf(x, 2)+powf(y, 2)));
                        if (y==0||y==x) { (x < 0)? y += ang : y -= ang; } // y-x==0
                        else { (y < 0)? x -= ang: x += ang; }
                        gle::attribf(x+.5, y+.5);       gle::attribf(x, y);
                }
                gle::end();

                pophudmatrix();
        }
    /*
        vector<hudevent> hudevents;

        void drawhudevents(fpsent *d, int w, int h) // not fully implemented yet
        {
                if (d != player1) return;
                if (d->state != CS_ALIVE)
                {
                        hudevents.shrink(0);
                        return;
                }
                float rw = ((float)w / ((float)h/1800.f)), rh = 1800;
                float iw = rw+512.f, ix = 256;

                loopi(hudevents.length())
                {
                        if (lastmillis-hudevents[i].millis > 2000)
                        {
                                hudevents.remove(i);
                                if (hudevents.length()) hudevents[0].millis = lastmillis;
                                continue;
                        }

                        float x = clamp((lastmillis-hudevents[i].millis)-1000.f, -1000.f, 1000.f);
                        x = ((x<0? min(x+500.f, 0.f): max(x-500.f, 0.f))+500)/1000*iw -ix;
                        float y = 350;

                        Texture *i_special = NULL;
                        switch (hudevents[i].type)
                        {
                                case HET_HEADSHOT:
                                        i_special = textureload("@{tig/rr-core}/texture/hud/.png", 0, true, false);
                                        break;
                                case HET_DIRECTHIT:
                                        i_special = textureload("@{tig/rr-core}/texture/hud/.png", 0, true, false);
                                        break;
                        }
                        float sw = i_special->xs/2,
                                  sh = i_special->ys;
//todo
                        glBindTexture(GL_TEXTURE_2D, i_special->id);
                        glColor4f(1.f, 1.f, 1.f, 1.f);

                        gle::defvertex(2);
                gle::deftexcoord0();
                gle::begin(GL_TRIANGLE_STRIP);
                        gle::attribf(0, 0); gle::attribf(x-sw,       y);
                        gle::attribf(1, 0); gle::attribf(x+sw,       y);
                        gle::attribf(0, 1); gle::attribf(x-sw,       y+sh);
                        gle::attribf(1, 1); gle::attribf(x+sw,       y+sh);
                        gle::end();
                        break;
                }
        }
    */
        float a_scale = 1.2f;
//#include "engine.h"
        void drawroundicon(int w,int h)
        {
                //static Texture *i_round = NULL;
                //if (!i_round) i_round = textureload("@{tig/rr-core}/texture/hud/round.png", 0, true, false);
                //static Texture *i_roundnum = NULL;
                //if (!i_roundnum) i_roundnum = textureload("@{tig/rr-core}/texture/hud/roundnum.png", 0, true, false);

                pushhudmatrix();
        hudmatrix.scale(a_scale, a_scale, 1);
        flushhudmatrix();
                float x = 230,
                      y = 1700/a_scale;// - i_round->ys/a_scale;
                float sw = 70,//i_round->xs,
                      sh = 70;//i_round->ys;

                // draw meter
                drawometer(/*min((float)remain/(float)roundtotal, 1.f)*/1.0f, 150, vec(x+120, y-80, 0), vec(1, 0, 0));

        //      glColor4f(1.f, 1.f, 1.f, 1.f); hudmatrix.d = vec4(1, 1, 1, 1);

                // draw "round" icon
                //glBindTexture(GL_TEXTURE_2D, i_round->id);
        settexture("@{tig/rr-core}/texture/hud/round.png");

        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(0, 0); gle::attribf(x,          y);
        gle::attribf(1, 0); gle::attribf(x+sw,       y);
        gle::attribf(0, 1); gle::attribf(x,          y+sh);
        gle::attribf(1, 1); gle::attribf(x+sw,       y+sh);
        gle::end();

                // draw round number (fingers)
                x += sw+10;
                y -= sh/2.5;
        float dmround = 4.0;
                float rxscale = min(740.f/(dmround*40-(dmround/5)*15), 1.f);
                x /= rxscale;
                hudmatrix.scale(rxscale, 1, 1);
        flushhudmatrix();
        settexture("@{tig/rr-core}/texture/hud/roundnum.png");
                //glBindTexture(GL_TEXTURE_2D, i_roundnum->id);
                sw = 70;//i_roundnum->xs;
                sh = 70;//i_roundnum->ys;

                for(int i=0; i<dmround; i++)
                {
            gle::defvertex(2);
            gle::deftexcoord0();
            gle::begin(GL_TRIANGLE_STRIP);
                        if(i%5 == 4)
                        {
                                float tempx = x-(50*3);
                                float tempy = y-20;

                                gle::attribf(0, 0); gle::attribf(tempx+(180.f),   tempy);
                                gle::attribf(1, 0); gle::attribf(tempx+(220.f),   tempy+(sh*.6f));
                                gle::attribf(0, 1); gle::attribf(tempx,           tempy+(sh*.8f));
                                gle::attribf(1, 1); gle::attribf(tempx+(40.f),    tempy+(sh*1.3f));

                                x += 25;
                        } else {
                                gle::attribf(0, 0); gle::attribf(i%5 < 2 ? x+sw : x, y);
                                gle::attribf(1, 0); gle::attribf(i%5 < 2 ? x : x+sw, y);
                                gle::attribf(0, 1); gle::attribf(i%5 < 2 ? x+sw : x, y+sh);
                                gle::attribf(1, 1); gle::attribf(i%5 < 2 ? x : x+sw, y+sh);
                                x += 40;
                        }
            gle::end();
                }
                pophudmatrix();
        }
    int ammohudup[3] = { GUN_CG, GUN_RL, GUN_GL },
        ammohuddown[3] = { GUN_RIFLE, GUN_SG, GUN_PISTOL },
        ammohudcycle[7] = { -1, -1, -1, -1, -1, -1, -1 };

    ICOMMAND_NOLUA(ammohudup, "V", (tagval *args, int numargs),
    {
        loopi(3) ammohudup[i] = i < numargs ? getweapon(args[i].getstr()) : -1;
    });

    ICOMMAND_NOLUA(ammohuddown, "V", (tagval *args, int numargs),
    {
        loopi(3) ammohuddown[i] = i < numargs ? getweapon(args[i].getstr()) : -1;
    });

    ICOMMAND_NOLUA(ammohudcycle, "V", (tagval *args, int numargs),
    {
        loopi(7) ammohudcycle[i] = i < numargs ? getweapon(args[i].getstr()) : -1;
    });

    VARP(ammohud, 0, 1, 1);

    void drawammohud(fpsent *d)
    {
        float x = HICON_X + 2*HICON_STEP, y = HICON_Y, sz = HICON_SIZE;
        pushhudmatrix();
        hudmatrix.scale(1/3.2f, 1/3.2f, 1);
        flushhudmatrix();
        float xup = (x+sz)*3.2f, yup = y*3.2f + 0.1f*sz;
        loopi(3)
        {
            int gun = ammohudup[i];
            if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
            drawicon(HICON_FIST+gun, xup, yup, sz);
            yup += sz;
        }
        float xdown = x*3.2f - sz, ydown = (y+sz)*3.2f - 0.1f*sz;
        loopi(3)
        {
            int gun = ammohuddown[3-i-1];
            if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
            ydown -= sz;
            drawicon(HICON_FIST+gun, xdown, ydown, sz);
        }
        int offset = 0, num = 0;
        loopi(7)
        {
            int gun = ammohudcycle[i];
            if(gun < GUN_FIST || gun > GUN_PISTOL) continue;
            if(gun == d->gunselect) offset = i + 1;
            else if(d->ammo[gun]) num++;
        }
        float xcycle = (x+sz/2)*3.2f + 0.5f*num*sz, ycycle = y*3.2f-sz;
        loopi(7)
        {
            int gun = ammohudcycle[(i + offset)%7];
            if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
            xcycle -= sz;
            drawicon(HICON_FIST+gun, xcycle, ycycle, sz);
        }
        pophudmatrix();
    }

    void drawammohud(fpsent *d, int w, int h)
    {
        float rw = ((float)w / ((float)h/1800.f)), rh = 1800, sz = HICON_SIZE;
                //glColor4f(1.f, 1.f, 1.f, 1.f);

                // draw ammo bar

                //static Texture *ammo_bar = NULL;
                //if (!ammo_bar) ammo_bar = textureload("@{tig/rr-core}/texture/hud/ammo_bar.png", 0, true, false);
                //glBindTexture(GL_TEXTURE_2D, ammo_bar->id);
        settexture("@{tig/rr-core}/texture/hud/ammo_bar.png");

                float aw = 100, ah = 100;//ammo_bar->xs * 1.9, ah = ammo_bar->ys * 1.9;
                float ax = rw - aw, ay = rh - ah;

                //glColor3f((m_teammode||d->infected)? 0.7: 1, d->infected? 1: (m_teammode? 0.7: 1), d->infected? 0.7: 1);
                //again: possible hudmatrix.d dunno test it

        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(0, 0); gle::attribf(ax     ,       ay);
                gle::attribf(1, 0); gle::attribf(ax+aw, ay);
                gle::attribf(0, 1); gle::attribf(ax     ,       ay+ah);
                gle::attribf(1, 1); gle::attribf(ax+aw, ay+ah);
                gle::end();

                pushhudmatrix();
                hudmatrix.scale(2.f, 2.f, 1.f);
        flushhudmatrix();
                // draw ammo count

                char tcx[10];
                int tcw, tch;
                sprintf(tcx, "%d", d->ammo[d->gunselect]);
                text_bounds(tcx, tcw, tch);
                draw_text(tcx, (int)rw/2 - 80 - tcw/2, (rh-260-tch)/2);

                // draw bullets

                //static Texture *ammo_bullet = NULL;
                //if (!ammo_bullet) ammo_bullet = textureload("@{tig/rr-core}/texture/hud/ammo_bullet.png", 0, true, false);
                //glBindTexture(GL_TEXTURE_2D, ammo_bullet->id);


                float step = 15,
                          sx = (int)rw/2 - 190,
                          sy = (rh-260-tch)/2;

                int btd;
                int max = GUN_AMMO_MAX(d->gunselect);
                if (max <= 21 && d->ammo[d->gunselect] <= 21) btd = d->ammo[d->gunselect];
                else btd = (int)ceil((float)min(d->ammo[d->gunselect], max)/max * 21.f);

                loopi(btd)
                {
                        gle::defvertex(2);
            gle::deftexcoord0();
            gle::begin(GL_TRIANGLE_STRIP);
                        gle::attribf(0, 0); gle::attribf(sx                             ,       sy);
                        gle::attribf(1, 0); gle::attribf(sx+20/*ammo_bullet->w*/,       sy);
                        gle::attribf(0, 1); gle::attribf(sx                             ,       sy+20/*ammo_bullet->h*/);
                        gle::attribf(1, 1); gle::attribf(sx+20/*ammo_bullet->w*/,       sy+20/*ammo_bullet->h*/);
                        gle::end();

                        sx -= step;
                }
                pophudmatrix();

                // draw weapon icons

        pushhudmatrix();
        float xup = (rw-1000+sz)*1.5f, yup = HICON_Y*1.5f+8;
        glScalef(1/1.5f, 1/1.5f, 1);
                loopi(NUMWEAPS)
                {
                        int gun = (i+1)%NUMWEAPS;
                        if (!WEAP_USABLE(gun)) continue;
                        if (gun == d->gunselect)
                        {
                                drawicon(guns[gun].icon, xup - 30, yup - 60, 180);
                                xup += sz * 1.5;
                                continue;
                        }
                        if (!d->ammo[gun]) continue;

            drawicon(guns[gun].icon, xup, yup, sz);
                        xup += sz * 1.5;
                }
        pophudmatrix();
        }
    void drawhudbody(float x, float y, float sx, float sy, float ty)
    {
                gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(0, ty); gle::attribf(x     ,       y);
                gle::attribf(1, ty); gle::attribf(x+sx, y);
                gle::attribf(0, 1);  gle::attribf(x     ,       y+sy);
                gle::attribf(1, 1);  gle::attribf(x+sx, y+sy);
                gle::end();
    }

        VARP(gunwaithud, 0, 1, 1);

        void drawcrosshairhud(fpsent *d, int w, int h)
        {
                if (!gunwaithud || d->gunwait <= 300) return;

                float mwait = ((float)(lastmillis-d->lastaction)*(float)(lastmillis-d->lastaction))/((float)d->gunwait*(float)d->gunwait);
                mwait = clamp(mwait, 0.f, 1.f);

                ////static Texture *gunwaitt = NULL;
                ////if (!gunwaitt) gunwaitt = textureload("@{tig/rr-core}/texture/hud/gunwait.png", 0, true, false);
                //static Texture *gunwaitft = NULL;
                //if (!gunwaitft) gunwaitft = textureload("@{tig/rr-core}/texture/hud/gunwait_filled.png", 0, true, false);

                float rw = ((float)w / ((float)h/1800.f)), rh = 1800;
                float scale = a_scale * 1.6;
                float x = (rw/scale)/2.f+100, y = (rh-40/*gunwaitft->ys*/*2.f)/(scale*2.f);

                pushhudmatrix();
                hudmatrix.scale(scale, scale, 1);
        flushhudmatrix();
                //glColor4f(1.0f, 1.0f, 1.0f, 0.5); hudmatrix.d = vec4( probably

                //glBindTexture(GL_TEXTURE_2D, gunwaitt->id);
                //drawhudbody(x, y, gunwaitt->xs, gunwaitt->ys, 0);
        settexture("@{tig/rr-core}/texture/hud/gunwait_filled.png");
                //glBindTexture(GL_TEXTURE_2D, gunwaitft->id);
                drawhudbody(x, y + (1-mwait) * 40/*gunwaitft->ys*/, 30/*gunwaitft->xs*/, mwait * 20/*gunwaitft->ys*/, 1-mwait);

                pophudmatrix();
        }

        VARP(hudplayers, 0, 1, 1);

        void drawhudplayers(fpsent *d, int w, int h)
        {
                #define loopscoregroup(o, b) \
                loopv(sg->players) \
                { \
                        fpsent *o = sg->players[i]; \
                        b; \
                }

                int numgroups = groupplayers();
                if (numgroups <= 1) return;

                if (!hudplayers) return;

                scoregroup *sg = NULL;
                loopv(groups) if (!strcmp(groups[i]->team, d->team)) sg = groups[0];
                if (!sg) return;

                int numplayers = min(sg->players.length(), 9);
                int x = 1800*w/h - 500, y = 540, step = curfont->scale+4;
                int iy = y + step*numplayers - 15;

                //glEnable(GL_BLEND);
                //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                //defaultshader->set();
        //hudshader->set();

                //static Texture *teamhudbg = NULL; //lol wtf you declare it as NULL and say if ! ? :D
                //if (!teamhudbg) teamhudbg = textureload("@{tig/rr-core}/texture/hud/teamhudbg.png", 0, true, false);
                //glBindTexture(GL_TEXTURE_2D, teamhudbg->id);
        settexture("@{tig/rr-core}/texture/hud/teamhudbg.png");

                //glColor4f(.2f, .2f, .6f, .2f);

                gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
                gle::attribf(0.f, 0.f); gle::attribf(x-10,  y-10);
                gle::attribf(1.f, 0.f); gle::attribf(x+460, y-10);
                gle::attribf(0.f, 1.f); gle::attribf(x-10,  iy);
                gle::attribf(1.f, 1.f); gle::attribf(x+460, iy);
                gle::end();

                string cname;
                stringformatter cfmt = stringformatter(cname);
                //settextscale(.6f);

                loopi(numplayers)
                {
                        fpsent *o = sg->players[i];
                        cfmt("%s: \fb%s", PClasses[o->pclass].name, colorname(o));
                        draw_text(cname, x, y);
                        y += step;
                }

                //settextscale(1.f);
        }

        int lastguts=0, lastgutschangemillis=0, lastgutschange=0;

        int pmillis = 0;

        float lasthp = 0, lastap = 0;
    void drawhudicons(fpsent *d, int w, int h)
        {
        if(d->state!=CS_DEAD)
        {
                        // draw guts
                        /*if(m_survivalb)
                        {
                                //int tw, th;
                                //text_bounds(gutss, tw, th);
                                int guts = (d==player1)? d->guts: lastguts;
                                //@todo: consider making the G in "Guts" lowercase
                                defformatstring(gutss)("\f1Guts: \f3%d", guts);
                                draw_text(gutss, w*1800/h - 420, 1350);

                                if (guts != lastguts)
                                {
                                        lastgutschange = guts-lastguts;
                                        lastgutschangemillis = lastmillis;
                                }
                                if (lastgutschange && lastmillis-lastgutschangemillis<2550)
                                {
                                        formatstring(gutss)(lastgutschange>0? "\fg+%d": "\fe%d", lastgutschange);
                                        draw_text(gutss, w*1800/h - 262, 1300, 255, 255, 255, 255-((lastmillis-lastgutschangemillis)/10));
                                }
                                lastguts = guts;
                        }*/

                        // draw ammo
            if(d->quadmillis) drawicon(HICON_QUAD, 80, 1200);
            if(ammohud) drawammohud(d, w, h);
                        //if(m_dmsp) drawroundicon(w,h);
                        drawcrosshairhud(d, w, h);

                        if (!m_insta)
                        {
                                //static Texture *h_body_fire = NULL;
                                //if (!h_body_fire) h_body_fire = textureload("@{tig/rr-core}/texture/hud/body_fire.png", 0, true, false);
                                //static Texture *h_body = NULL;
                                //if (!h_body) h_body = textureload("@{tig/rr-core}/texture/hud/body.png", 0, true, false);
                                //static Texture *h_body_white = NULL;
                                //if (!h_body_white) h_body_white = textureload("@{tig/rr-core}/texture/hud/body_white.png", 0, true, false);

                                pushhudmatrix();
                                hudmatrix.scale(a_scale, a_scale, 1);

                                int bx = HICON_X * 2,
                                        by = HICON_Y/a_scale - 50/*h_body->ys*//a_scale;
                                int bt = lastmillis - pmillis;

                                // draw fire
                                float falpha = d->onfire? (float)(abs(((lastmillis-d->burnmillis)%900)-450)+50)/500: 0.f;
                                if (falpha)
                                {
                                        //glBindTexture(GL_TEXTURE_2D, h_body_fire->id);
                    settexture("@{tig/rr-core}/texture/hud/body_fire.png");
                    //i uncommented all these textureload stuff because glBindTexture wont work anymore
                    //i would recommend to write a function void gettexturesize(int &h, int &w); or sth
                                        //glColor4f(1.f, 1.f, 1.f, falpha); hudmatrix.d probably

                                        // -20 works for "some reason"
                                        drawhudbody(bx, HICON_Y/a_scale - 30/*h_body_fire->ys*//a_scale - 20,
                                                                50/*h_body_fire->xs*/, 30/*h_body_fire->ys*/, 0);
                                }

                                // draw health
                                lasthp += float((d->health>lasthp)? 1: (d->health<lasthp)? -1: 0) * min(curtime/10.f, float(abs(d->health-lasthp)));
                                lasthp = min(lasthp, (float)d->maxhealth);
                                float b = max(lasthp, 0.f) / d->maxhealth, ba = 1;
                                if (b <= .4f)
                                {
                                        ba = ((float)bt-500) / 500 + 0.2;
                                        if (ba < 0) ba *= -1;
                                        if (bt >= 1000) pmillis = lastmillis;
                                }
                settexture("@{tig/rr-core}/texture/hud/body.png");
                                //glBindTexture(GL_TEXTURE_2D, h_body->id);
                                drawhudbody(bx, by, 50/*h_body->xs*/, 40/*h_body->ys*/, 0);
                settexture("@{tig/rr-core}/texture/hud/body_white.png");
                                //glBindTexture(GL_TEXTURE_2D, h_body_white->id);
                                glColor4f(1-b, max(b-0.6f, 0.f), 0.0, ba);
                                drawhudbody(bx, by + (1-b) * 30/*h_body_white->ys*/, 46/*h_body_white->xs*/, b * 30/*h_body_white->ys*/, 1-b);

                                // draw armour
                                //static Texture *h_body_armour = NULL;
                                //if (!h_body_armour) h_body_armour = textureload("@{tig/rr-core}/texture/hud/body_armour.png", 0, true, false);
                                //glBindTexture(GL_TEXTURE_2D, h_body_armour->id);
                settexture("@{tig/rr-core}/texture/hud/body_armour.png");
                                float a_alpha = .8f;

                                if (d->armour)
                                {
                                        int maxarmour = d->armour;
                                        /*if (d->armourtype == A_BLUE)
                                        {
                                                maxarmour = 50;
                                                //glColor4f(0.f, 0.f, 1.f, a_alpha); //all wont work but try .d
                                        }
                                        else if (d->armourtype == A_GREEN)
                                        {
                                                maxarmour = 100;
                                                //glColor4f(0.f, .7f, 0.f, a_alpha);
                                        }
                                        else if (d->armourtype == A_YELLOW)
                                        {
                                                maxarmour = 200;
                                                //glColor4f(1.f, 0.5f, 0.f, a_alpha);
                                        }*/

                                        lastap += ((d->armour>lastap)? 1: (d->armour<lastap)? -1: 0) * min(curtime/10.f, float(abs(d->armour-lastap)));
                                        lastap = min(lastap, (float)maxarmour);
                                        float c = max(lastap, 0.f) / (float)maxarmour;
                                        drawhudbody(bx, by + (1-c) * 30/*h_body_armour->ys*/, 48/*h_body_armour->xs*/, c * 30/*h_body_armour->ys*/, 1-c);
                                }

                                pophudmatrix();

                                char tst[20];
                                int tw, th;
                                if (d->armour > 0)
                                        sprintf(tst, "%d/%d", d->health, d->armour);
                                else
                                        sprintf(tst, "%d", d->health);
                                text_bounds(tst, tw, th);
                                draw_text(tst, bx + (/*h_body->xs*/30*a_scale)/2 - tw/2, by*a_scale + /*h_body->ys*/46*a_scale + 6);
                        }
                        drawhudplayers(d, w, h);
                        //drawhudevents(d, w, h);
       }
    }

    void gameplayhud(int w, int h)
    {
        pushhudmatrix();
        hudmatrix.scale(h/1800.0f, h/1800.0f, 1);
        flushhudmatrix();

        if(player1->state==CS_SPECTATOR)
        {
            int pw, ph, tw, th, fw, fh;
            text_bounds("  ", pw, ph);
            text_bounds("SPECTATOR", tw, th);
            th = max(th, ph);
            fpsent *f = followingplayer();
            text_bounds(f ? colorname(f) : " ", fw, fh);
            fh = max(fh, ph);
            draw_text("SPECTATOR", w*1800/h - tw - pw, 1650 - th - fh);
            if(f)
            {
                int color = f->state!=CS_DEAD ? 0xFFFFFF : 0x606060;
                if(f->privilege)
                {
                    color = f->privilege>=PRIV_ADMIN ? 0xFF8000 : 0x40FF80;
                    if(f->state==CS_DEAD) color = (color>>1)&0x7F7F7F;
                }
                draw_text(colorname(f), w*1800/h - fw - pw, 1650 - fh, (color>>16)&0xFF, (color>>8)&0xFF, color&0xFF);
            }
        }

        fpsent *d = hudplayer();
        if(d->state!=CS_EDITING)
        {
            drawhudicons(d, w, h);

            if(cmode) cmode->drawhud(d, w, h);
        }

        pophudmatrix();
    }

    int clipconsole(int w, int h)
    {
        if(cmode) return cmode->clipconsole(w, h);
        return 0;
    }

    VARP(teamcrosshair, 0, 1, 1);
    VARP(hitcrosshair, 0, 425, 1000);

    const char *defaultcrosshair(int index)
    {
        switch(index)
        {
            case 2: return "@{tig/rr-core}/crosshair/hit.png";
            case 1: return "@{tig/rr-core}/crosshair/teammate.png";
            default: return "@{tig/rr-core}/crosshair/circle.png";
        }
    }

    int selectcrosshair(float &r, float &g, float &b)
    {
        fpsent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_DEAD) return -1;

        if(d->state!=CS_ALIVE) return 0;

        int crosshair = 0;
        if(lasthit && lastmillis - lasthit < hitcrosshair) crosshair = 2;
        else if(teamcrosshair)
        {
            dynent *o = intersectclosest(d->o, worldpos, d);
            if(o && o->type==ENT_PLAYER && isteam(((fpsent *)o)->team, d->team))
            {
                crosshair = 1;
                r = g = 0;
            }
        }

        if(crosshair!=1 && !editmode)
        {
            if(d->health<=25) { r = 1.0f; g = b = 0; }
            else if(d->health<=50) { r = 1.0f; g = 0.5f; b = 0; }
        }
        if(d->gunwait) { r *= 0.5f; g *= 0.5f; b *= 0.5f; }
        return crosshair;
    }

    void lighteffects(dynent *e, vec &color, vec &dir)
    {
#if 0
        fpsent *d = (fpsent *)e;
        if(d->state!=CS_DEAD && d->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
    }

    bool serverinfostartcolumn(g3d_gui *g, int i)
    {
        static const char *names[] =  { "ping ", "players ", "mode ", "map ", "time ", "master ", "host ", "port ", "description " };
        static const float struts[] = { 7,       7,          12.5f,   14,      7,      8,         14,      7,       24.5f };
        if(size_t(i) >= sizeof(names)/sizeof(names[0])) return false;
        g->pushlist();
        g->text(names[i], 0xFFFF80, !i ? " " : NULL);
        if(struts[i]) g->strut(struts[i]);
        g->mergehits(true);
        return true;
    }

    void serverinfoendcolumn(g3d_gui *g, int i)
    {
        g->mergehits(false);
        g->column(i);
        g->poplist();
    }

    const char *mastermodecolor(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodecolors)/sizeof(mastermodecolors[0])) ? mastermodecolors[n-MM_START] : unknown;
    }

    const char *mastermodeicon(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodeicons)/sizeof(mastermodeicons[0])) ? mastermodeicons[n-MM_START] : unknown;
    }

    bool serverinfoentry(g3d_gui *g, int i, const char *name, int port, const char *sdesc, const char *map, int ping, const vector<int> &attr, int np)
    {
        if(ping < 0 || attr.empty() || attr[0]!=PROTOCOL_VERSION)
        {
            switch(i)
            {
                case 0:
                    if(g->button(" ", 0xFFFFDD, "serverunk")&G3D_UP) return true;
                    break;

                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                    if(g->button(" ", 0xFFFFDD)&G3D_UP) return true;
                    break;

                case 6:
                    if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
                    break;

                case 7:
                    if(g->buttonf("%d ", 0xFFFFDD, NULL, port)&G3D_UP) return true;
                    break;

                case 8:
                    if(ping < 0)
                    {
                        if(g->button(sdesc, 0xFFFFDD)&G3D_UP) return true;
                    }
                    else if(g->buttonf("[%s protocol] ", 0xFFFFDD, NULL, attr.empty() ? "unknown" : (attr[0] < PROTOCOL_VERSION ? "older" : "newer"))&G3D_UP) return true;
                    break;
            }
            return false;
        }

        switch(i)
        {
            case 0:
            {
                const char *icon = attr.inrange(3) && np >= attr[3] ? "serverfull" : (attr.inrange(4) ? mastermodeicon(attr[4], "serverunk") : "serverunk");
                if(g->buttonf("%d ", 0xFFFFDD, icon, ping)&G3D_UP) return true;
                break;
            }

            case 1:
                if(attr.length()>=4)
                {
                    if(g->buttonf(np >= attr[3] ? "\f3%d/%d " : "%d/%d ", 0xFFFFDD, NULL, np, attr[3])&G3D_UP) return true;
                }
                else if(g->buttonf("%d ", 0xFFFFDD, NULL, np)&G3D_UP) return true;
                break;

            case 2:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=2 ? server::modename(attr[1], "") : "")&G3D_UP) return true;
                break;

            case 3:
                if(g->buttonf("%.25s ", 0xFFFFDD, NULL, map)&G3D_UP) return true;
                break;

            case 4:
                if(attr.length()>=3 && attr[2] > 0)
                {
                    int secs = clamp(attr[2], 0, 59*60+59),
                        mins = secs/60;
                    secs %= 60;
                    if(g->buttonf("%d:%02d ", 0xFFFFDD, NULL, mins, secs)&G3D_UP) return true;
                }
                else if(g->buttonf(" ", 0xFFFFDD)&G3D_UP) return true;
                break;
            case 5:
                if(g->buttonf("%s%s ", 0xFFFFDD, NULL, attr.length()>=5 ? mastermodecolor(attr[4], "") : "", attr.length()>=5 ? server::mastermodename(attr[4], "") : "")&G3D_UP) return true;
                break;

            case 6:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
                break;

            case 7:
                if(g->buttonf("%d ", 0xFFFFDD, NULL, port)&G3D_UP) return true;
                break;

            case 8:
                if(g->buttonf("%.25s", 0xFFFFDD, NULL, sdesc)&G3D_UP) return true;
                break;
        }
        return false;
    }

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    const char *savedconfig() { return "@{@User}/config/rr/config.cfg"; }
    const char *restoreconfig() { return "@{@User}/config/rr/restore.cfg"; }
    const char *defaultconfig() { return "@{tig/rr-core}/config/defaults.cfg"; }
    const char *autoexec() { return "@{@User}/config/rr/autoexec.cfg"; }
    const char *savedservers() { return "@{@User}/config/rr/servers.cfg"; }

    void loadconfigs()
    {
        execfile("@{@User}/config/rr/auth.cfg", false);
    }
}

