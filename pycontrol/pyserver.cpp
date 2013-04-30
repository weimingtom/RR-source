/*

 This is a modified version of the original Sauerbraten source code.
 
*/

#include "sbpy.h"
#include "servermodule.h"
#include "server.h"

#include <iostream>

#include <signal.h>

namespace game
{
    void parseoptions(vector<const char *> &args)
    {
        loopv(args)
#ifndef STANDALONE
            if(!game::clientoption(args[i]))
#endif
            if(!server::serveroption(args[i]))
                conoutf(CON_ERROR, "unknown command-line option: %s", args[i]);
    }

	int getdamageranged(int damage, int gun, bool headshot, bool quad, vec from, vec to)
	{
		if (!WEAP_IS_EXPLOSIVE(gun) && WEAP(gun,range)>20)
		{
			damage = (float)damage * (1.f-clamp(from.dist(to)/(float)WEAP(gun,range), 0.f, 0.9f));
		}
		return damage*(quad ? 4 : 1)*(headshot? GUN_HEADSHOT_MUL: 1.0f);
	}
}

namespace ai
{
	vector<char *> botnames;

	ICOMMAND(registerbot, "s", (const char *s), botnames.add(copystring(new string, s)));
}

extern ENetAddress masteraddress;

namespace server
{

    bool notgotitems = true;        // true when map has changed and waiting for clients to send item
    int gamemode = 0;
    int gamemillis = 0, gamelimit = 0, nextexceeded = 0;
    bool gamepaused = false;

    string smapname = "";
    int interm = 0;
    bool mapreload = false;
    enet_uint32 lastsend = 0;
    int mastermode = MM_OPEN, mastermask = MM_PRIVSERV;
    int currentmaster = -1;
    stream *mapdata = NULL;

    vector<uint> allowedips;
    vector<clientinfo *> connects, clients, bots;
    vector<worldstate *> worldstates;
    bool reliablemessages = false;
    bool checkexecqueue = false;
    bool allow_modevote = false;

#define MAXDEMOS 5
    vector<demofile> demos;

    bool demonextmatch = false;
    stream *demotmp = NULL, *demorecord = NULL, *demoplayback = NULL;
    int nextplayback = 0, demomillis = 0;


#define SERVMODE 1
//#include "capture.h"
#include "ctf.h"
#include "infection.h"
#include "survival.h"

    //captureservmode capturemode;
    ctfservmode ctfmode;
    infectionservmode infectionmode;
    survivalservmode survivalmode;
    servmode *smode = NULL;

    SVAR(serverdesc, "");
    SVAR(serverpass, "");
    SVAR(adminpass, "");
    SVAR(pyscriptspath, "");
    SVAR(pyconfigpath, "Config/");
    VARF(publicserver, 0, 0, 2, {
        switch(publicserver)
        {
             case 0: default: mastermask = MM_PRIVSERV; break;
             case 1: mastermask = MM_PUBSERV; break;
             case 2: mastermask = MM_COOPSERV; break;
        }
    });
    SVAR(servermotd, "");
	VAR(maxzombies, 0, 20, 100);
	VAR(flaglimit, 0, 10, 100);
	VAR(timelimit, 0, 10, 1000);
	VAR(allowweaps, 0, 0, 2);

    void *newclientinfo() { return new clientinfo; }
    void deleteclientinfo(void *ci) { delete (clientinfo *)ci; }

    clientinfo *getinfo(int n)
    {
        if(n < MAXCLIENTS) return (clientinfo *)getclientinfo(n);
        n -= MAXCLIENTS;
        return bots.inrange(n) ? bots[n] : smode? smode->getcinfo(n+MAXCLIENTS): NULL;
    }

    vector<server_entity> sents;
    vector<savedscore> scores;

    int msgsizelookup(int msg)
    {
        static int sizetable[NUMSV] = { -1 };
        if(sizetable[0] < 0)
        {
            memset(sizetable, -1, sizeof(sizetable));
            for(const int *p = msgsizes; *p >= 0; p += 2) sizetable[p[0]] = p[1];
        }
        return msg >= 0 && msg < NUMSV ? sizetable[msg] : -1;
    }

    const char *modename(int n, const char *unknown)
    {
        if(m_valid(n)) return gamemodes[n - STARTGAMEMODE].name;
        return unknown;
    }

    const char *mastermodename(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodenames)/sizeof(mastermodenames[0])) ? mastermodenames[n-MM_START] : unknown;
    }

    const char *privname(int type)
    {
        switch(type)
        {
            case PRIV_ADMIN: return "admin";
            case PRIV_MASTER: return "master";
            default: return "unknown";
        }
    }

    void sendservmsg(const char *s) { sendf(-1, 1, "ris", N_SERVMSG, s); }
	
	void sendservmsgf(int cn, const char *f, ...)
	{
		defvformatstring(str, f, f);
		sendf(-1, 1, "ris", N_SERVMSG, str);
	}

    void resetitems()
    {
        sents.shrink(0);
        //cps.reset();
    }

    bool serveroption(const char *arg)
    {
        if(arg[0]=='-') switch(arg[1])
        {
            case 'n': setsvar("serverdesc", &arg[2]); return true;
            case 'y': setsvar("serverpass", &arg[2]); return true;
            case 'p': setsvar("adminpass", &arg[2]); return true;
            case 'o': setvar("publicserver", atoi(&arg[2])); return true;
            case 'g': setvar("serverbotlimit", atoi(&arg[2])); return true;
            case 's': setsvar("pyscriptspath", &arg[2]); return true;
            case 'a': setsvar("pyconfigpath", &arg[2]); return true;
        }
        return false;
    }

    bool serverinit()
    {
        smapname[0] = '\0';
        resetitems();

        // Initialize python modules
        if(!SbPy::init("sauer_server", pyscriptspath, "sbserver"))
            return false;
        return true;
    }

    int numclients(int exclude, bool nospec, bool noai, bool priv)
    {
        int n = 0;
        loopv(clients)
        {
           clientinfo *ci = clients[i];
           if(i!=exclude && (!nospec || ci->state.state!=CS_SPECTATOR || (priv && (ci->privilege || ci->local))) && (!noai || ci->state.aitype == AI_NONE)) n++;
        }
        return n;
    }

    bool duplicatename(clientinfo *ci, char *name)
    {
        if(!name) name = ci->name;
        loopv(clients) if(clients[i]!=ci && !strcmp(name, clients[i]->name)) return true;
        return false;
    }

    const char *colorname(clientinfo *ci, char *name = NULL)
    {
        if(!name) name = ci->name;
        if(name[0] && !duplicatename(ci, name) && ci->state.aitype == AI_NONE) return name;
        static string cname[3];
        static int cidx = 0;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx])(ci->state.aitype == AI_NONE ? "%s \fs\f5(%d)\fr" : "%s \fs\f5[%d]\fr", name, ci->clientnum);
        return cname[cidx];
    }

    bool canspawnitem(int type) { return !m_noitems && (type>=I_AMMO && type<=I_QUAD && !m_noammo); }
	
    int spawntime(int type)
    {
        if(m_classicsp) return INT_MAX;
        int np = numclients(-1, true, false);
        np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
        int sec = 0;
        switch(type)
        {
            case I_AMMO:
			case I_AMMO2: case I_AMMO3:
			case I_AMMO4: sec = np*4; break;
            case I_HEALTH: case I_HEALTH2:
			case I_HEALTH3: sec = np*5; break;
            case I_GREENARMOUR:
            case I_YELLOWARMOUR: sec = 20; break;
            case I_MORTAR: case I_QUAD: sec = 40+rnd(40); break;
        }
        return sec*1000;
    }

    bool delayspawn(int type)
    {
        switch(type)
        {
            case I_GREENARMOUR:
            case I_YELLOWARMOUR:
                return !m_classicsp;
            case I_QUAD:
                return true;
            default:
                return false;
        }
    }


    bool pickup(int i, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if((m_timed && gamemillis>=gamelimit) || !sents.inrange(i) || !sents[i].spawned) return false;
        clientinfo *ci = getinfo(sender);
        if(!ci || (!ci->local && !ci->state.canpickup(sents[i].type))) return false;
        sents[i].spawned = false;
        sents[i].spawntime = spawntime(sents[i].type);
        sendf(-1, 1, "ri3", N_ITEMACC, i, sender);
        ci->state.pickup(sents[i].type);
        return true;
    }

    clientinfo *choosebestclient(float &bestrank)
    {
        clientinfo *best = NULL;
        bestrank = -1;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.timeplayed<0) continue;
            float rank = ci->state.state!=CS_SPECTATOR ? ci->state.effectiveness/max(ci->state.timeplayed, 1) : -1;
            if(!best || rank > bestrank) { best = ci; bestrank = rank; }
        }
        return best;
    }

    bool setteam(clientinfo *ci, char *team)
    {
        if(!ci || !strcmp(ci->team, team)) return false;
        if(smode && ci->state.state==CS_ALIVE) smode->changeteam(ci, ci->team, team);
        copystring(ci->team, team);
        aiman::changeteam(ci);
        sendf(-1, 1, "riis", N_SETTEAM, ci->clientnum, ci->team);
        return true;
    }

    void switchteam(clientinfo *ci, char *team, int sender)
    {
        if(ci && strcmp(ci->team, team))
        {
            if(m_teammode && smode && !smode->canchangeteam(ci, ci->team, team))
                sendf(sender, 1, "riis", N_SETTEAM, sender, ci->team);
            else
            {
                if(smode && ci->state.state==CS_ALIVE)
                    smode->changeteam(ci, ci->team, team);
                copystring(ci->team, team);
                aiman::changeteam(ci);
                sendf(-1, 1, "riis", N_SETTEAM, sender, ci->team);
            }
        }
    }

    /* This is a special purpose function for setting teams pre-game
       which wont be effected by the autoteam function. */
    bool pregame_setteam(clientinfo *ci, char *team)
    {
        if(!ci) return false;
        ci->state.timeplayed = -1;
        if(!strcmp(ci->team, team)) return true;
        copystring(ci->team, team, MAXTEAMLEN+1);
        sendf(-1, 1, "riis", N_SETTEAM, ci->clientnum, team);
        return true;
    }

    void autoteam()
    {
        static const char *teamnames[2] = {TEAM_0, TEAM_1};
        vector<clientinfo *> team[2];
        float teamrank[2] = {0, 0};
        int remaining = clients.length();
        SbPy::triggerEvent("autoteam", 0);
        // We arent going to set clients already assigned a team
        clientinfo *ci;
        loopv(clients)
        {
            ci = clients[i];
            if(ci->state.timeplayed<0)
                remaining--;
        }
        // Do autoteam
        for(int round = 0; remaining>=0; round++)
        {
            int first = round&1, second = (round+1)&1, selected = 0;
            while(teamrank[first] <= teamrank[second])
            {
                float rank;
                clientinfo *ci = choosebestclient(rank);
                if(!ci) break;
                if(smode && smode->hidefrags()) rank = 1;
                else if(selected && rank<=0) break;
                ci->state.timeplayed = -1;
                team[first].add(ci);
                if(rank>0) teamrank[first] += rank;
                selected++;
                if(rank<=0) break;
            }
            if(!selected) break;
            remaining -= selected;
        }
        loopi(sizeof(team)/sizeof(team[0]))
        {
            loopvj(team[i])
            {
                clientinfo *ci = team[i][j];
                if(!strcmp(ci->team, teamnames[i])) continue;
                copystring(ci->team, teamnames[i], MAXTEAMLEN+1);
                sendf(-1, 1, "riisi", N_SETTEAM, ci->clientnum, teamnames[i], -1);
            }
        }
    }

    struct teamrank
    {
        const char *name;
        float rank;
        int clients;

        teamrank(const char *name) : name(name), rank(0), clients(0) {}
    };

    const char *chooseworstteam(const char *suggest = NULL, clientinfo *exclude = NULL)
    {
        teamrank teamranks[2] = { teamrank(TEAM_0), teamrank(TEAM_1) };
        const int numteams = sizeof(teamranks)/sizeof(teamranks[0]);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci==exclude || ci->state.aitype!=AI_NONE || ci->state.state==CS_SPECTATOR || !ci->team[0]) continue;
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            ci->state.lasttimeplayed = lastmillis;

            loopj(numteams) if(!strcmp(ci->team, teamranks[j].name))
            {
                teamrank &ts = teamranks[j];
                ts.rank += ci->state.effectiveness/max(ci->state.timeplayed, 1);
                ts.clients++;
                break;
            }
        }
        teamrank *worst = &teamranks[numteams-1];
        loopi(numteams-1)
        {
            teamrank &ts = teamranks[i];
            if(smode && smode->hidefrags())
            {
                if(ts.clients < worst->clients || (ts.clients == worst->clients && ts.rank < worst->rank)) worst = &ts;
            }
            else if(ts.rank < worst->rank || (ts.rank == worst->rank && ts.clients < worst->clients)) worst = &ts;
        }
        return worst->name;
    }

    int welcomepacket(packetbuf &p, clientinfo *ci);
    void sendwelcome(clientinfo *ci);

/** Demo Recording **/
    void writedemo(int chan, void *data, int len)
    {
        if(!demorecord) return;
        int stamp[3] = { gamemillis, chan, len };
        lilswap(stamp, 3);
        demorecord->write(stamp, sizeof(stamp));
        demorecord->write(data, len);
    }

    void recordpacket(int chan, void *data, int len)
    {
        writedemo(chan, data, len);
    }

    void enddemorecord()
    {
        if(!demorecord) return;

        DELETEP(demorecord);

        if(!demotmp) return;

        int len = demotmp->size();
        if(demos.length()>=MAXDEMOS)
        {
            delete[] demos[0].data;
            demos.remove(0);
        }
        demofile &d = demos.add();
        time_t t = time(NULL);
        char *timestr = ctime(&t), *trim = timestr + strlen(timestr);
        while(trim>timestr && isspace(*--trim)) *trim = '\0';
        formatstring(d.info)("%s: %s, %s, %.2f%s", timestr, modename(gamemode), smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
        defformatstring(msg)("demo \"%s\" recorded", d.info);
		formatstring(d.name)(smapname);
        sendservmsg(msg);
        d.data = new uchar[len];
        d.len = len;
        demotmp->seek(0, SEEK_SET);
        demotmp->read(d.data, len);
        DELETEP(demotmp);
        SbPy::triggerEventInt("demo_recorded", demos.ulen - 1);
    }

    void setupdemorecord()
    {
        if(!m_mp(gamemode) || m_edit) return;

        demotmp = opentempfile("demorecord", "w+b");
        if(!demotmp) return;

        stream *f = opengzfile(NULL, "wb", demotmp);
        if(!f) { DELETEP(demotmp); return; }

        sendservmsg("recording demo");

        demorecord = f;

        demoheader hdr;
        memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
        hdr.version = DEMO_VERSION;
        hdr.protocol = PROTOCOL_VERSION;
        lilswap(&hdr.version, 2);
        demorecord->write(&hdr, sizeof(demoheader));

        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        welcomepacket(p, NULL);
        writedemo(1, p.buf, p.len);
    }

    void listdemos(int cn)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_SENDDEMOLIST);
        putint(p, demos.length());
        loopv(demos) sendstring(demos[i].info, p);
        sendpacket(cn, 1, p.finalize());
    }

    void cleardemos(int n)
    {
        if(!n)
        {
            loopv(demos) delete[] demos[i].data;
            demos.shrink(0);
            sendservmsg("cleared all demos");
        }
        else if(demos.inrange(n-1))
        {
            delete[] demos[n-1].data;
            demos.remove(n-1);
            defformatstring(msg)("cleared demo %d", n);
            sendservmsg(msg);
        }
    }

    void senddemo(int cn, int num)
    {
        if(!num) num = demos.length();
        if(!demos.inrange(num-1)) return;
        demofile &d = demos[num-1];
        sendf(cn, 2, "rism", N_SENDDEMO, d.name, d.len, d.data);
    }

    void enddemoplayback()
    {
        if(!demoplayback) return;
        DELETEP(demoplayback);

        loopv(clients) sendf(clients[i]->clientnum, 1, "ri3", N_DEMOPLAYBACK, 0, clients[i]->clientnum);

        sendservmsg("demo playback finished");

        loopv(clients) sendwelcome(clients[i]);
    }

    void setupdemoplayback()
    {
        if(demoplayback) return;
        demoheader hdr;
        string msg;
        msg[0] = '\0';
        defformatstring(file)("%s/%s.dmo", "game::demodir", smapname);
        demoplayback = opengzfile(file, "rb");
        if(!demoplayback) formatstring(msg)("could not read demo \"%s\"", file);
        else if(demoplayback->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
            formatstring(msg)("\"%s\" is not a demo file", file);
        else
        {
            lilswap(&hdr.version, 2);
            if(hdr.version!=DEMO_VERSION) formatstring(msg)("demo \"%s\" requires an %s version of Revelade Revolution", file, hdr.version<DEMO_VERSION ? "older" : "newer");
            else if(hdr.protocol!=PROTOCOL_VERSION) formatstring(msg)("demo \"%s\" requires an %s version of Revelade Revolution", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
        }
        if(msg[0])
        {
            DELETEP(demoplayback);
            sendservmsg(msg);
            return;
        }

        formatstring(msg)("playing demo \"%s\"", file);
        sendservmsg(msg);

        demomillis = 0;
        sendf(-1, 1, "ri3", N_DEMOPLAYBACK, 1, -1);

        if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        lilswap(&nextplayback, 1);
    }

    void readdemo()
    {
        if(!demoplayback || gamepaused) return;
        demomillis += curtime;
        while(demomillis>=nextplayback)
        {
            int chan, len;
            if(demoplayback->read(&chan, sizeof(chan))!=sizeof(chan) ||
               demoplayback->read(&len, sizeof(len))!=sizeof(len))
            {
                enddemoplayback();
                return;
            }
            lilswap(&chan, 1);
            lilswap(&len, 1);
            ENetPacket *packet = enet_packet_create(NULL, len, 0);
            if(!packet || demoplayback->read(packet->data, len)!=len)
            {
                if(packet) enet_packet_destroy(packet);
                enddemoplayback();
                return;
            }
            sendpacket(-1, chan, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
            {
                enddemoplayback();
                return;
            }
            lilswap(&nextplayback, 1);
        }
    }

    void stopdemo()
    {
        if(m_demo) enddemoplayback();
        else enddemorecord();
    }

    void pausegame(bool val)
    {
        if(gamepaused==val) return;
        gamepaused = val;
        sendf(-1, 1, "rii", N_PAUSEGAME, gamepaused ? 1 : 0);
    }

    void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen)
    {
        char buf[2*sizeof(string)];
        formatstring(buf)("%d %d ", cn, sessionid);
        copystring(&buf[strlen(buf)], pwd);
        if(!hashstring(buf, result, maxlen)) *result = '\0';
    }

    bool checkpassword(clientinfo *ci, const char *wanted, const char *given)
    {
        string hash;
        hashpassword(ci->clientnum, ci->sessionid, wanted, hash, sizeof(hash));
        return !strcmp(hash, given);
    }

    void revokemaster(clientinfo *ci)
    {
        ci->privilege = PRIV_NONE;
        if(ci->state.state==CS_SPECTATOR && !ci->local) aiman::removeai(ci);
    }

    void setcimaster(clientinfo *ci)
    {
        loopv(clients) if(ci!=clients[i] && clients[i]->privilege<=PRIV_MASTER) revokemaster(clients[i]);
        if(ci)
        {
            ci->privilege = PRIV_MASTER;
            currentmaster = ci->clientnum;
            SbPy::triggerEventInt("player_claimed_master", ci->clientnum);
        }
        else
        {
            currentmaster = -1;
        }
        sendf(-1, 1, "ri4", N_CURRENTMASTER, currentmaster, currentmaster >= 0 ? ci->privilege : 0, mastermode);
    }

    void setciadmin(clientinfo *ci)
    {
        string msg;
        loopv(clients) if(ci!=clients[i] && clients[i]->privilege<=PRIV_MASTER) revokemaster(clients[i]);
        ci->privilege = PRIV_ADMIN;
        currentmaster = ci->clientnum;
        SbPy::triggerEventInt("player_claimed_admin", ci->clientnum);
    }

    void resetpriv(clientinfo *ci)
    {
        if(!ci || !ci->privilege) return;
        if(ci->privilege == PRIV_MASTER)
            SbPy::triggerEventInt("player_released_master", ci->clientnum);
        else
            SbPy::triggerEventInt("player_released_admin", ci->clientnum);
        ci->privilege = PRIV_NONE;
        currentmaster = -1; 
    }

    savedscore &findscore(clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip && !ci->local) return *(savedscore *)0;
        if(!insert)
        {
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->clientnum != ci->clientnum && getclientip(oi->clientnum) == ip && !strcmp(oi->name, ci->name))
                {
                    oi->state.timeplayed += lastmillis - oi->state.lasttimeplayed;
                    oi->state.lasttimeplayed = lastmillis;
                    static savedscore curscore;
                    curscore.save(oi->state);
                    return curscore;
                }
            }
        }
        loopv(scores)
        {
            savedscore &sc = scores[i];
            if(sc.ip == ip && !strcmp(sc.name, ci->name)) return sc;
        }
        if(!insert) return *(savedscore *)0;
        savedscore &sc = scores.add();
        sc.ip = ip;
        copystring(sc.name, ci->name);
        return sc;
    }

    void savescore(clientinfo *ci)
    {
        savedscore &sc = findscore(ci, true);
        if(&sc) sc.save(ci->state);
    }

    int checktype(int type, clientinfo *ci)
    {
        if(ci && ci->local) return type;
        // only allow edit messages in coop-edit mode
        if(type>=N_EDITENT && type<=N_EDITVAR && !m_edit) return -1;
        // server only messages
        static const int servtypes[] = {
			N_SERVINFO, N_INITCLIENT, N_WELCOME, N_MAPRELOAD, N_SERVMSG, N_DAMAGE, N_HITPUSH, N_SHOTFX,
			N_EXPLODEFX, N_DIED, N_SPAWNSTATE, N_FORCEDEATH, N_ITEMACC, N_ITEMSPAWN, N_TIMEUP, N_CDIS,
			N_CURRENTMASTER, N_PONG, N_RESUME, N_BASESCORE, N_BASEINFO, N_BASEREGEN, N_ANNOUNCE,
			N_SENDDEMOLIST, N_SENDDEMO, N_DEMOPLAYBACK, N_SENDMAP, N_DROPFLAG, N_SCOREFLAG, N_RETURNFLAG,
			N_RESETFLAG, N_INVISFLAG, N_CLIENT, N_AUTHCHAL, N_INITAI, N_SETONFIRE, N_SETCLASS,
			N_INFECT, N_SURVINIT, N_SURVREASSIGN, N_SURVSPAWNSTATE, N_SURVNEWROUND, N_GUTS };
        if(ci)
        {
            loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
            if(type < N_EDITENT || type > N_EDITVAR || !m_edit)
            {
				//todo: review
                if(type != N_POS && ++ci->overflow >= 250) return -2;
            }
        }
        return type;
    }

    void cleanworldstate(ENetPacket *packet)
    {
        loopv(worldstates)
        {
            worldstate *ws = worldstates[i];
            if(ws->positions.inbuf(packet->data) || ws->messages.inbuf(packet->data)) ws->uses--;
            else continue;
            if(!ws->uses)
            {
                delete ws;
                worldstates.remove(i);
            }
            break;
        }
    }

    void flushclientposition(clientinfo &ci)
    {
        if(ci.position.empty() || (!hasnonlocalclients() && !demorecord)) return;
        packetbuf p(ci.position.length(), 0);
        p.put(ci.position.getbuf(), ci.position.length());
        ci.position.setsize(0);
        sendpacket(-1, 0, p.finalize(), ci.ownernum);
    }

    void addclientstate(worldstate &ws, clientinfo &ci)
    {
        if(ci.position.empty()) ci.posoff = -1;
        else
        {
            ci.posoff = ws.positions.length();
            ws.positions.put(ci.position.getbuf(), ci.position.length());
            ci.poslen = ws.positions.length() - ci.posoff;
            ci.position.setsize(0);
        }
        if(ci.messages.empty()) ci.msgoff = -1;
        else
        {
            ci.msgoff = ws.messages.length();
            putint(ws.messages, N_CLIENT);
            putint(ws.messages, ci.clientnum);
            putuint(ws.messages, ci.messages.length());
            ws.messages.put(ci.messages.getbuf(), ci.messages.length());
            ci.msglen = ws.messages.length() - ci.msgoff;
            ci.messages.setsize(0);
        }
    }

    bool buildworldstate()
    {
        worldstate &ws = *new worldstate;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            ci.overflow = 0;
            addclientstate(ws, ci);
            loopv(ci.bots)
            {
                clientinfo &bi = *ci.bots[i];
                addclientstate(ws, bi);
                if(bi.posoff >= 0)
                {
                    if(ci.posoff < 0) { ci.posoff = bi.posoff; ci.poslen = bi.poslen; }
                    else ci.poslen += bi.poslen;
                }
                if(bi.msgoff >= 0)
                {
                    if(ci.msgoff < 0) { ci.msgoff = bi.msgoff; ci.msglen = bi.msglen; }
                    else ci.msglen += bi.msglen;
                }
            }
        }
		if (smode) smode->buildworldstate(ws);
        int psize = ws.positions.length(), msize = ws.messages.length();
        if(psize)
        {
            recordpacket(0, ws.positions.getbuf(), psize);
            ucharbuf p = ws.positions.reserve(psize);
            p.put(ws.positions.getbuf(), psize);
            ws.positions.addbuf(p);
        }
        if(msize)
        {
            recordpacket(1, ws.messages.getbuf(), msize);
            ucharbuf p = ws.messages.reserve(msize);
            p.put(ws.messages.getbuf(), msize);
            ws.messages.addbuf(p);
        }
        ws.uses = 0;
        if(psize || msize) loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            ENetPacket *packet;
            if(psize && (ci.posoff<0 || psize-ci.poslen>0))
            {
                packet = enet_packet_create(&ws.positions[ci.posoff<0 ? 0 : ci.posoff+ci.poslen],
                                            ci.posoff<0 ? psize : psize-ci.poslen,
                                            ENET_PACKET_FLAG_NO_ALLOCATE);
                sendpacket(ci.clientnum, 0, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
                else { ++ws.uses; packet->freeCallback = cleanworldstate; }
            }

            if(msize && (ci.msgoff<0 || msize-ci.msglen>0))
            {
                packet = enet_packet_create(&ws.messages[ci.msgoff<0 ? 0 : ci.msgoff+ci.msglen],
                                            ci.msgoff<0 ? msize : msize-ci.msglen,
                                            (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
                sendpacket(ci.clientnum, 1, packet);
                if(!packet->referenceCount) enet_packet_destroy(packet);
                else { ++ws.uses; packet->freeCallback = cleanworldstate; }
            }
        }
        reliablemessages = false;
        if(!ws.uses)
        {
            delete &ws;
            return false;
        }
        else
        {
            worldstates.add(&ws);
            return true;
        }
    }

    bool sendpackets(bool force)
    {
        if(clients.empty() || (!hasnonlocalclients() && !demorecord)) return false;
        enet_uint32 curtime = enet_time_get()-lastsend;
        if(curtime<33 && !force) return false;
        bool flush = buildworldstate();
        lastsend += curtime - (curtime%33);
        return flush;
    }

    template<class T>
    void sendstate(gamestate &gs, T &p)
    {
        putint(p, gs.lifesequence);
        putint(p, gs.guts);
        putint(p, gs.health);
        putint(p, gs.maxhealth);
        putint(p, gs.armour);
        putint(p, gs.armourtype);
        putint(p, gs.playerclass);
        putint(p, gs.gunselect);
        loopi(WEAP_PISTOL-WEAP_SLUGSHOT+1) putint(p, gs.ammo[WEAP_SLUGSHOT+i]);
    }

    void spawnstate(clientinfo *ci)
    {
        gamestate &gs = ci->state;

		if (m_infection)
		{
			gs.infected = (m_infection)? rnd(2): 0;
			sendf(-1, 1, "ri3", N_INFECT, gs.infected? ci->clientnum: -1, !gs.infected? ci->clientnum: -1);
		}

        gs.spawnstate(gamemode);
        gs.lifesequence = (gs.lifesequence + 1)&0x7F;
    }

    void sendspawn(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        spawnstate(ci);
        sendf(ci->ownernum, 1, "ri2i8v", N_SPAWNSTATE, ci->clientnum, gs.lifesequence, gs.guts,
            gs.health, gs.maxhealth,
            gs.armour, gs.armourtype,
            gs.playerclass, gs.gunselect, WEAP_PISTOL-WEAP_SLUGSHOT+1, &gs.ammo[WEAP_SLUGSHOT]);
        gs.lastspawn = gamemillis;
    }

    void sendwelcome(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        int chan = welcomepacket(p, ci);
        sendpacket(ci->clientnum, chan, p.finalize());
    }

    void putinitclient(clientinfo *ci, packetbuf &p)
    {
        if(ci->state.aitype != AI_NONE)
        {
            putint(p, N_INITAI);
            putint(p, ci->clientnum);
            putint(p, ci->ownernum);
            putint(p, ci->state.aitype);
            putint(p, ci->state.skill);
            putint(p, ci->playermodel);
            putint(p, ci->state.playerclass);
            sendstring(ci->name, p);
            sendstring(ci->team, p);
        }
        else
        {
            putint(p, N_INITCLIENT);
            putint(p, ci->clientnum);
            sendstring(ci->name, p);
            sendstring(ci->team, p);
            putint(p, ci->playermodel);
            putint(p, ci->state.playerclass);
        }
    }

    void welcomeinitclient(packetbuf &p, int exclude = -1)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->connected || ci->clientnum == exclude) continue;

            putinitclient(ci, p);
        }
    }

    int welcomepacket(packetbuf &p, clientinfo *ci)
    {
        int hasmap = (m_edit && (clients.length()>1 || (ci && ci->local))) || (smapname[0] && (!m_timed || gamemillis<gamelimit || (ci && ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || numclients(ci ? ci->clientnum : -1, true, true, true)));
        putint(p, N_WELCOME);
        putint(p, hasmap);
		putint(p, allowweaps);
        if(hasmap)
        {
            putint(p, N_MAPCHANGE);
            sendstring(smapname, p);
            putint(p, gamemode);
            putint(p, notgotitems ? 1 : 0);
            if(!ci || (m_timed && smapname[0]))
            {
                putint(p, N_TIMEUP);
                putint(p, gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0);
            }
            if(!notgotitems)
            {
                putint(p, N_ITEMLIST);
                loopv(sents) if(sents[i].spawned)
                {
                    putint(p, i);
                    putint(p, sents[i].type);
                }
                putint(p, -1);
            }
        }
        if(currentmaster >= 0 || mastermode != MM_OPEN)
        {
            putint(p, N_CURRENTMASTER);
            putint(p, currentmaster);
            clientinfo *m = currentmaster >= 0 ? getinfo(currentmaster) : NULL;
            putint(p, m ? m->privilege : 0);
            putint(p, mastermode);
        }
        if(gamepaused)
        {
            putint(p, N_PAUSEGAME);
            putint(p, 1);
        }
        if(ci)
        {
            putint(p, N_SETTEAM);
            putint(p, ci->clientnum);
            sendstring(ci->team, p);
            putint(p, -1);
        }
        if(ci && (m_demo || m_mp(gamemode)) && ci->state.state!=CS_SPECTATOR)
        {
            if(smode && !smode->canspawn(ci, true))
            {
                ci->state.state = CS_DEAD;
                putint(p, N_FORCEDEATH);
                putint(p, ci->clientnum);
                sendf(-1, 1, "ri2x", N_FORCEDEATH, ci->clientnum, ci->clientnum);
            }
            else
            {
                gamestate &gs = ci->state;
                spawnstate(ci);
                putint(p, N_SPAWNSTATE);
                putint(p, ci->clientnum);
                sendstate(gs, p);
                gs.lastspawn = gamemillis;
            }
        }
        if(ci && ci->state.state==CS_SPECTATOR)
        {
            putint(p, N_SPECTATOR);
            putint(p, ci->clientnum);
            putint(p, 1);
            sendf(-1, 1, "ri3x", N_SPECTATOR, ci->clientnum, 1, ci->clientnum);
        }
        if(!ci || clients.length()>1)
        {
            putint(p, N_RESUME);
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(ci && oi->clientnum==ci->clientnum) continue;
                putint(p, oi->clientnum);
                putint(p, oi->state.state);
                putint(p, oi->state.frags);
                putint(p, oi->state.flags);
                putint(p, oi->state.quadmillis);
                sendstate(oi->state, p);
            }
            putint(p, -1);
            welcomeinitclient(p, ci ? ci->clientnum : -1);
        }
        if(ci && smode) smode->initclient(ci, p, true);
        return 1;
    }

    bool restorescore(clientinfo *ci)
    {
        //if(ci->local) return false;
        savedscore &sc = findscore(ci, false);
        if(&sc)
        {
            sc.restore(ci->state);
            return true;
        }
        return false;
    }

    void sendresume(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        sendf(-1, 1, "ri3i9i2vi", N_RESUME, ci->clientnum,
            gs.state, gs.frags, gs.flags, gs.quadmillis,
            gs.lifesequence, gs.guts,
            gs.health, gs.maxhealth,
            gs.armour, gs.armourtype,
            gs.playerclass, gs.gunselect,
			WEAP_PISTOL-WEAP_SLUGSHOT+1, &gs.ammo[WEAP_SLUGSHOT], -1);
    }

    void sendinitclient(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p);
        sendpacket(-1, 1, p.finalize(), ci->clientnum);
    }

    void changemap(const char *s, int mode)
    {
        SbPy::triggerEvent("map_changed_pre", 0);
        stopdemo();
        pausegame(false);
        if(smode) smode->reset(false);
        aiman::clearai();

        mapreload = false;
        gamemode = mode;
        gamemillis = 0;
        gamelimit = timelimit*(m_overtime? 90000: 60000);
        interm = 0;
        copystring(smapname, s);
        resetitems();
        notgotitems = true;
        scores.shrink(0);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
			ci->state.guts = 0;
        }

        if(!m_mp(gamemode)) kicknonlocalclients(DISC_PRIVATE);

        if(m_teammode) autoteam();
		else if (m_oneteam) loopv(clients) strcpy(clients[i]->team, TEAM_0);

        //if(m_capture) smode = &capturemode; else
		if(m_ctf) smode = &ctfmode;
        else if(m_infection) smode = &infectionmode;
        else if(m_survival) smode = &survivalmode;
        else smode = NULL;
        if(smode) smode->reset(false);

        if(m_timed && smapname[0]) sendf(-1, 1, "ri2", N_TIMEUP, gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->mapchange();
            ci->state.lasttimeplayed = lastmillis;
            if(m_mp(gamemode) && ci->state.state!=CS_SPECTATOR) sendspawn(ci);
        }

        aiman::changemap();

        if(m_demo)
        {
            if(clients.length()) setupdemoplayback();
        }
        else if(demonextmatch)
        {
            demonextmatch = false;
            setupdemorecord();
        }
        SbPy::triggerEventStrInt("map_changed", smapname, mode);
    }

    void setmap(const char *s, int mode)
    {
        sendf(-1, 1, "risii", N_MAPCHANGE, s, mode, 1);
        changemap(s, mode);
    }

    void setmastermode(int mm)
    {
        mastermode = mm;
        allowedips.setsize(0);
        if(mastermode>=MM_PRIVATE)
        {
            loopv(clients) allowedips.add(getclientip(clients[i]->clientnum));
        }
        sendf(-1, 1, "rii", N_MASTERMODE, mastermode);
        SbPy::triggerEventInt("server_mastermode_changed", mastermode);
    }

    void forcemap(const char *map, int mode)
    {
        stopdemo();
        if(hasnonlocalclients() && !mapreload)
        {
            defformatstring(msg)("local player forced %s on map %s", modename(mode), map);
            sendservmsg(msg);
        }
        sendf(-1, 1, "risii", N_MAPCHANGE, map, mode, 1);
        changemap(map, mode);
    }

    void setgamemins(int mins)
    {
        gamelimit = gamemillis + (mins * 60000) + 1;
    }

    void checkintermission()
    {
        if(gamemillis >= gamelimit && !interm)
        {
            SbPy::triggerEvent("intermission_begin", 0);
            sendf(-1, 1, "ri2", N_TIMEUP, 0);
            if(smode) smode->intermission();
            interm = gamemillis + 10000;
        }
    }

    void startintermission() { gamelimit = min(gamelimit, gamemillis); checkintermission(); }

    void dodamage(clientinfo *target, clientinfo *actor, int damage, int gun, const vec &hitpush = vec(0, 0, 0), bool special = false)
    {
		if (!actor) return;
        gamestate &ts = target->state;
        ts.dodamage(damage);
        if (damage>0) actor->state.damage += damage;
        sendf(-1, 1, "ri7", N_DAMAGE, target->clientnum, actor->clientnum, damage, ts.armour, ts.health, gun);
        if(target==actor) target->setpushed();
        else if(target!=actor && !hitpush.iszero() && damage>0)
        {
            ivec v = vec(hitpush).rescale(DNF);
            sendf(ts.health<=0 ? -1 : target->ownernum, 1, "ri7", N_HITPUSH, target->clientnum, gun, damage, v.x, v.y, v.z);
            target->setpushed();
        }
		if (m_survivalb && actor!=target && isteam(actor->team, target->team)) actor->state.teamshooter = true;
        if(ts.health<=0)
        {
			if (m_survival && target->state.aitype==AI_ZOMBIE)
			{
				const zombietype &zt = zombietypes[((zombie*)target)->ztype];
				actor->state.guts += zt.health/zt.freq*2;
				sendf(actor->ownernum, 1, "ri3", N_GUTS, actor->clientnum, actor->state.guts);
			}
            target->state.deaths++;
            if(actor!=target && isteam(actor->team, target->team))
            {
                actor->state.teamkills++;
				if (!target->state.teamshooter)
				{
					actor->state.teamkilled = true;
					actor->state.guts -= 700;
					sendf(actor->ownernum, 1, "ri3", N_GUTS, actor->clientnum, actor->state.guts);
				}
                if(actor->clientnum != target->clientnum)
                    SbPy::triggerEventIntInt("player_teamkill", actor->clientnum, target->clientnum);
            }
            int fragvalue = smode ? smode->fragvalue(target, actor) : (target==actor || isteam(target->team, actor->team) ? -1 : 1);
            actor->state.frags += fragvalue;
            if(fragvalue>0)
            {
                int friends = 0, enemies = 0; // note: friends also includes the fragger
                if(m_teammode) loopv(clients) if(strcmp(clients[i]->team, actor->team)) enemies++; else friends++;
                else { friends = 1; enemies = clients.length()-1; }
                actor->state.effectiveness += fragvalue*friends/float(max(enemies, 1));
            }
            sendf(-1, 1, "ri6", N_DIED, target->clientnum, actor->clientnum, actor->state.frags, gun, special);
            target->position.setsize(0);
            if(smode) smode->died(target, actor);
            ts.state = CS_DEAD;
            ts.lastdeath = gamemillis;
            SbPy::triggerEventIntInt("player_frag", actor->clientnum, target->clientnum);
            // don't issue respawn yet until DEATHMILLIS has elapsed
            // ts.respawn();
        }
    }

    void suicide(clientinfo *ci, int type)
    {
        gamestate &gs = ci->state;
        if(gs.state!=CS_ALIVE) return;
        ci->state.frags += smode ? smode->fragvalue(ci, ci) : -1;
        ci->state.deaths++;
        sendf(-1, 1, "ri6", N_DIED, ci->clientnum, ci->clientnum, gs.frags, -1-type, 0);
        ci->position.setsize(0);
        if(smode) smode->died(ci, NULL);
        gs.state = CS_DEAD;
        gs.respawn();
    }


    void suicideevent::process(clientinfo *ci)
    {
        suicide(ci, this->type);
    }

	
	int getradialdamage(int damage, int gun, bool headshot, bool quad, float distmul)
	{
		damage = int((float)damage * (1.f-min(max(distmul - 0.1f, 0.f), 1.f)));
		return damage*(quad ? 4 : 1)*(headshot? GUN_HEADSHOT_MUL: 1.0f);
	}

    void explodeevent::process(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        switch(WEAPONI(gun))
        {
            case WEAP_ROCKETL:
                if(!gs.rockets.remove(id)) return;
                break;

            case WEAP_GRENADIER:
                if(!gs.grenades.remove(id)) return;
                break;

        }
        sendf(-1, 1, "ri4x", N_EXPLODEFX, ci->clientnum, gun, id, ci->ownernum);
        gs.shots++;
        loopv(hits)
        {
            hitinfo &h = hits[i];
            clientinfo *target = getinfo(h.target);
            if(!target || target->state.state!=CS_ALIVE || (h.lifesequence!=target->state.lifesequence && target->state.aitype != AI_ZOMBIE) || h.dist<0 || (WEAP_IS_EXPLOSIVE(gun) && h.dist>WEAP(gun,projradius))) continue;

            bool dup = false;
            loopj(i) if(hits[j].target==h.target) { dup = true; break; }
            if(dup) continue;
			int damage = getradialdamage(WEAP(gun,damage), gun, h.headshot, gs.quadmillis, h.dist/(WEAP_IS_EXPLOSIVE(gun)? WEAP(gun,projradius): WEAP(gun,range)));
            if (!WEAP_IS_EXPLOSIVE(gun)) direct = h.headshot;
            if(target==ci) damage /= GUN_EXP_SELFDAMDIV;
            dodamage(target, ci, damage, gun, h.dir, direct);
        }
    }

    void shotevent::process(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        int wait = millis - gs.lastshot;
        if(!gs.isalive(gamemillis) || wait<gs.gunwait || (WEAP(gun,range) && from.dist(to) > WEAP(gun,range) + 1)) return;
		if (!CAN_SHOOT_WITH2(gs,gun)) return;
        gs.ammo[WEAPONI(gun)] = max(gs.ammo[WEAPONI(gun)]-WEAP(gun,numshots), 0);

        gs.lastshot = millis;
        gs.gunwait = WEAP(gun,attackdelay);
		if (numrays != WEAP(gun,numrays)) return;
        sendf(-1, 1, "rii9ivx", N_SHOTFX, ci->clientnum, gun, id,
                int(from.x*DMF), int(from.y*DMF), int(from.z*DMF),
                int(to.x*DMF), int(to.y*DMF), int(to.z*DMF),
				numrays, numrays*sizeof(ivec)/sizeof(int), &rays,
                ci->ownernum);

		int damage;
        gs.shots++;
        switch(WEAPONI(gun))
        {
            case WEAP_ROCKETL: gs.rockets.add(id); break;
            case WEAP_GRENADIER: gs.grenades.add(id); break;
			case WEAP_FLAMEJET: /*gs.flames.add(id);*/ break;
            default:
            {
                int totalrays = 0, maxrays = WEAP(gun,numrays);
                loopv(hits)
                {
                    hitinfo &h = hits[i];
                    clientinfo *target = getinfo(h.target);
                    if(!target || target->state.state!=CS_ALIVE || (h.lifesequence!=target->state.lifesequence && target->state.aitype != AI_ZOMBIE) || h.rays<1 || h.dist > WEAP(gun,range) + 1) continue;

					damage = h.rays*game::getdamageranged(WEAP(gun,damage), gun, headshot, gs.quadmillis, from, target->state.o);
					gs.shotdamage += damage;
                    gs.hits++;

                    totalrays += h.rays;
                    if(totalrays>maxrays) continue;
                    dodamage(target, ci, damage, gun, h.dir, h.headshot);
                }
                break;
            }
        }
    }

    void pickupevent::process(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        if(m_mp(gamemode) && !gs.isalive(gamemillis)) return;
        pickup(ent, ci->clientnum);
    }

    bool gameevent::flush(clientinfo *ci, int fmillis)
    {
        process(ci);
        return true;
    }

    bool timedevent::flush(clientinfo *ci, int fmillis)
    {
        if(millis > fmillis) return false;
        else if(millis >= ci->lastevent)
        {
            ci->lastevent = millis;
            process(ci);
        }
        return true;
    }

    void clearevent(clientinfo *ci)
    {
        delete ci->events.remove(0);
    }

    void flushevents(clientinfo *ci, int millis)
    {
        while(ci->events.length())
        {
            gameevent *ev = ci->events[0];
            if(ev->flush(ci, millis)) clearevent(ci);
            else break;
        }
    }

    void processevents()
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(curtime>0 && ci->state.quadmillis) ci->state.quadmillis = max(ci->state.quadmillis-curtime, 0);
            flushevents(ci, gamemillis);
        }
    }

    void cleartimedevents(clientinfo *ci)
    {
        int keep = 0;
        loopv(ci->events)
        {
            if(ci->events[i]->keepable())
            {
                if(keep < i)
                {
                    for(int j = keep; j < i; j++) delete ci->events[j];
                    ci->events.remove(keep, i - keep);
                    i = keep;
                }
                keep = i+1;
                continue;
            }
        }
        while(ci->events.length() > keep) delete ci->events.pop();
        ci->timesync = false;
    }

    bool ispaused() { return gamepaused; }

    void serverupdate()
    {
        if(!gamepaused) gamemillis += curtime;

        if(m_demo) readdemo();
        else if(!gamepaused && (!m_timed || gamemillis < gamelimit))
        {
            processevents();
            if(curtime)
            {
                loopv(sents) if(sents[i].spawntime) // spawn entities when timer reached
                {
                    int oldtime = sents[i].spawntime;
                    sents[i].spawntime -= curtime;
                    if(sents[i].spawntime<=0)
                    {
                        sents[i].spawntime = 0;
                        sents[i].spawned = true;
                        sendf(-1, 1, "ri2", N_ITEMSPAWN, i);
                    }
                    else if(sents[i].spawntime<=10000 && oldtime>10000 && (sents[i].type==I_QUAD))
                    {
                        sendf(-1, 1, "ri2", N_ANNOUNCE, sents[i].type);
                    }
                }
            }
            aiman::checkai();
            if(smode) smode->update();
        }

        loopv(connects) if(totalmillis-connects[i]->connectmillis>15000) disconnect_client(connects[i]->clientnum, DISC_TIMEOUT);

        SbPy::update();

        if(!gamepaused && m_timed && smapname[0] && gamemillis-curtime>0) checkintermission();
        if(interm > 0 && gamemillis>interm)
        {
            if(demorecord) enddemorecord();
            interm = -1;
            SbPy::triggerEvent("intermission_ended", 0);
            //if(clients.length()) sendf(-1, 1, "ri", N_MAPRELOAD);
        }

		loopv(clients) if (clients[i]->state.onfire)
		{
			gamestate &gs = clients[i]->state;
			int burnmillis = gamemillis-gs.burnmillis;
			if (gs.state == CS_ALIVE && gamemillis-gs.lastburnpain >= clamp(burnmillis, 200, 1000))
			{
				int damage = min(WEAP(WEAP_FLAMEJET,damage)*1000/max(burnmillis, 1000), gs.health)*(gs.fireattacker->state.quadmillis? 4: 1);
				dodamage(clients[i], gs.fireattacker, damage, WEAP_FLAMEJET);
				gs.lastburnpain = gamemillis;
			}
			if (burnmillis > 4000)
			{
				gs.onfire = false;
				sendf(-1, 1, "ri5", N_SETONFIRE, clients[i]->state.fireattacker->clientnum, clients[i]->clientnum, WEAP_FLAMEJET, 0);
			}
		}
    }

    void sendmapreload()
    {
        if(clients.length()) sendf(-1, 1, "ri", N_MAPRELOAD);
    }

    struct crcinfo
    {
        int crc, matches;

        crcinfo(int crc, int matches) : crc(crc), matches(matches) {}

        static int compare(const crcinfo *x, const crcinfo *y)
        {
            if(x->matches > y->matches) return -1;
            if(x->matches < y->matches) return 1;
            return 0;
        }
    };

    void checkmaps(int req = -1)
    {
        if(m_edit || !smapname[0]) return;
        vector<crcinfo> crcs;
        int total = 0, unsent = 0, invalid = 0;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_SPECTATOR || ci->state.aitype != AI_NONE) continue;
            total++;
            if(!ci->clientmap[0])
            {
                if(ci->mapcrc < 0) invalid++;
                else if(!ci->mapcrc) unsent++;
            }
            else
            {
                crcinfo *match = NULL;
                loopvj(crcs) if(crcs[j].crc == ci->mapcrc) { match = &crcs[j]; break; }
                if(!match) crcs.add(crcinfo(ci->mapcrc, 1));
                else match->matches++;
            }
        }
        if(total - unsent < min(total, 4)) return;
        crcs.sort(crcinfo::compare);
        string msg;
	// TODO: move modified map messages into python
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_SPECTATOR || ci->state.aitype != AI_NONE || ci->clientmap[0] || ci->mapcrc >= 0 || (req < 0 && ci->warned)) continue;
            SbPy::triggerEventInt("player_modified_map", ci->clientnum);
            formatstring(msg)("%s has modified map \"%s\"", colorname(ci), smapname);
			//todo: prompt to download map
            sendf(req, 1, "ris", N_SERVMSG, msg);
            if(req < 0) ci->warned = true;
        }
        if(crcs.empty() || crcs.length() < 2) return;
        loopv(crcs)
        {
            crcinfo &info = crcs[i];
            if(i || info.matches <= crcs[i+1].matches) loopvj(clients)
            {
                clientinfo *ci = clients[j];
                if(ci->state.state==CS_SPECTATOR || ci->state.aitype != AI_NONE || !ci->clientmap[0] || ci->mapcrc != info.crc || (req < 0 && ci->warned)) continue;
                SbPy::triggerEventInt("player_modified_map", ci->clientnum);
                formatstring(msg)("%s has modified map \"%s\"", colorname(ci), smapname);
				//todo: prompt to download map
                sendf(req, 1, "ris", N_SERVMSG, msg);
                if(req < 0) ci->warned = true;
            }
        }
    }

    void sendservinfo(clientinfo *ci)
    {
        sendf(ci->clientnum, 1, "ri5s", N_SERVINFO, ci->clientnum, PROTOCOL_VERSION, ci->sessionid, serverpass[0] ? 1 : 0, serverdesc);
    }

    bool spectate(clientinfo *spinfo, bool val, int spectator)
    {
	bool spectated = false, unspectated = false;
        if(spinfo->state.state!=CS_SPECTATOR && val)
        {
             if(spinfo->state.state==CS_ALIVE) suicide(spinfo);
             if(smode) smode->leavegame(spinfo);
             spinfo->state.state = CS_SPECTATOR;
             spinfo->state.timeplayed += lastmillis - spinfo->state.lasttimeplayed;
             if(!spinfo->local && !spinfo->privilege) aiman::removeai(spinfo);
	     spectated = true;
        }
        else if(spinfo->state.state==CS_SPECTATOR && !val)
        {
             spinfo->state.state = CS_DEAD;
             spinfo->state.respawn();
             spinfo->state.lasttimeplayed = lastmillis;
             aiman::addclient(spinfo);
             if(spinfo->clientmap[0] || spinfo->mapcrc) checkmaps();
	     unspectated = true;
         }
         else
             return false;
         sendf(-1, 1, "ri3", N_SPECTATOR, spectator, val);
         if(!val && mapreload && !spinfo->privilege && !spinfo->local) sendf(spectator, 1, "ri", N_MAPRELOAD);
	 if(spectated)
            SbPy::triggerEventInt("player_spectated", spinfo->clientnum);
	 else if(unspectated)
            SbPy::triggerEventInt("player_unspectated", spinfo->clientnum);
	 return true;
    }

    void noclients()
    {
        aiman::clearai();
        SbPy::triggerEvent("no_clients", 0);
    }

    void localconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;
        ci->local = true;

        connects.add(ci);
        sendservinfo(ci);
    }

    void localdisconnect(int n)
    {
        if(m_demo) enddemoplayback();
        clientdisconnect(n);
    }

    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;

        connects.add(ci);
        if(!m_mp(gamemode)) return DISC_PRIVATE;
        sendservinfo(ci);
        return DISC_NONE;
    }

    void clientdisconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        SbPy::triggerEventInt("player_disconnect", n);
        SbPy::triggerEventInt("player_disconnect_post", n);
        if(ci->connected)
        {
            if(ci->privilege) resetpriv(ci);
            if(smode) smode->leavegame(ci, true);
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            savescore(ci);
            sendf(-1, 1, "ri2", N_CDIS, n);
            clients.removeobj(ci);
            aiman::removeai(ci);
            if(!numclients(-1, false, true)) noclients(); // bans clear when server empties
        }
        else connects.removeobj(ci);
    }

    int reserveclients() { return 3; }

    int allowconnect(clientinfo *ci, const char *pwd)
    {
        if(!m_mp(gamemode)) return DISC_PRIVATE;
        if(serverpass[0])
        {
            if(!checkpassword(ci, serverpass, pwd)) return DISC_PRIVATE;
            return DISC_NONE;
        }
        if(adminpass[0] && checkpassword(ci, adminpass, pwd)) return DISC_NONE;
        if(numclients(-1, false, true)>=maxclients) return DISC_MAXCLIENTS;
        uint ip = getclientip(ci->clientnum);
        if(mastermode>=MM_PRIVATE && allowedips.find(ip)<0) return DISC_PRIVATE;
        if(!SbPy::triggerPolicyEventIntString("connect_private", ci->clientnum, pwd)) return DISC_PRIVATE;
        if(!SbPy::triggerPolicyEventIntString("connect_kick", ci->clientnum, pwd)) return DISC_KICK;
        return DISC_NONE;
    }

    bool allowbroadcast(int n)
    {
        clientinfo *ci = getinfo(n);
        return ci && ci->connected;
    }

    clientinfo *findauth(uint id)
    {
        loopv(clients) if(clients[i]->authreq == id) return clients[i];
        return NULL;
    }

    void authchallenged(uint id, const char *val)
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, "", id, val);
    }

    void challengeauth(clientinfo *ci, uint id, const char *val)
    {
        if(!ci) return;
        sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, "", id, val);
    }

    uint nextauthreq = 0;

    void tryauth(clientinfo *ci, const char *user)
    {
        SbPy::triggerEventIntString("player_auth_request", ci->clientnum, user);
        return;
    }

    void answerchallenge(clientinfo *ci, uint id, char *val)
    {
        SbPy::triggerEventIntIntString("player_auth_challenge_response", ci->clientnum, id, val);
        return;
    }

    void receivefile(int sender, uchar *data, int len)
    {
        if(!m_edit || len > 1024*1024) return;
        clientinfo *ci = getinfo(sender);
        if(ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) return;
        if(mapdata) DELETEP(mapdata);
        if(!len) return;
        mapdata = opentempfile("mapdata", "w+b");
        if(!mapdata) { sendf(sender, 1, "ris", N_SERVMSG, "failed to open temporary file for map"); return; }
        mapdata->write(data, len);
        SbPy::triggerEventInt("player_uploaded_map", sender);
    }

    void sendclipboard(clientinfo *ci)
    {
        if(!ci->lastclipboard || !ci->clipboard) return;
        bool flushed = false;
        loopv(clients)
        {
            clientinfo &e = *clients[i];
            if(e.clientnum != ci->clientnum && e.needclipboard >= ci->lastclipboard)
            {
                if(!flushed) { flushserver(true); flushed = true; }
                sendpacket(e.clientnum, 1, ci->clipboard);
            }
        }
    }

    void parsepacket(int sender, int chan, packetbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender<0) return;
        char text[MAXTRANS];
        int type;
        clientinfo *ci = sender>=0 ? getinfo(sender) : NULL, *cq = ci, *cm = ci;
        if(ci && !ci->connected)
        {
            if(chan==0) return;
            else if(chan!=1 || getint(p)!=N_CONNECT) { disconnect_client(sender, DISC_TAGT); return; }
            else
            {
                getstring(text, p);
                filtertext(text, text, false, MAXNAMELEN);
                if(!text[0]) copystring(text, "unnamed");
                copystring(ci->name, text, MAXNAMELEN+1);

                getstring(text, p);

                SbPy::triggerEventInt("player_connect_pre", ci->clientnum);
                SbPy::triggerEventInt("player_connect", ci->clientnum);

                int disc = allowconnect(ci, text);
                if(disc)
                {
                    disconnect_client(sender, disc);
                    return;
                }

                ci->playermodel = getint(p);
                ci->state.playerclass = getint(p);

                if(m_demo) enddemoplayback();

                connects.removeobj(ci);
                clients.add(ci);

                ci->connected = true;
                ci->needclipboard = totalmillis;
                if(mastermode>=MM_LOCKED) ci->state.state = CS_SPECTATOR;
                ci->state.lasttimeplayed = lastmillis;

                const char *worst = m_teammode ? chooseworstteam(text, ci) : NULL;
                copystring(ci->team, worst ? worst : TEAM_0, MAXTEAMLEN+1);

                sendwelcome(ci);
                if(restorescore(ci)) sendresume(ci);
                sendinitclient(ci);

                aiman::addclient(ci);

                if(m_demo) setupdemoplayback();

            }
        }
        else if(chan==2)
        {
            receivefile(sender, p.buf, p.maxlen);
            return;
        }

        if(p.packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;
        #define QUEUE_AI clientinfo *cm = cq;
        #define QUEUE_MSG { if(cm && (!cm->local || demorecord || hasnonlocalclients())) while(curmsg<p.length()) cm->messages.add(p.buf[curmsg++]); }
        #define QUEUE_BUF(body) { \
            if(cm && (!cm->local || demorecord || hasnonlocalclients())) \
            { \
                curmsg = p.length(); \
                { body; } \
            } \
        }

        #define QUEUE_INT(n) QUEUE_BUF(putint(cm->messages, n))
        #define QUEUE_UINT(n) QUEUE_BUF(putuint(cm->messages, n))
        #define QUEUE_STR(text) QUEUE_BUF(sendstring(text, cm->messages))
        int curmsg;
        while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), ci))
        {
            case N_POS:
            {
                int pcn = getuint(p);
                p.get();
                uint flags = getuint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                vec pos;
                loopk(3)
                {
                    int n = p.get(); n |= p.get()<<8; if(flags&(1<<k)) { n |= p.get()<<16; if(n&0x800000) n |= -1<<24; }
                    pos[k] = n/DMF;
                }
                loopk(3) p.get();
                int mag = p.get(); if(flags&(1<<3)) mag |= p.get()<<8;
                int dir = p.get(); dir |= p.get()<<8;
                vec vel = vec((dir%360)*RAD, (clamp(dir/360, 0, 180)-90)*RAD).mul(mag/DVELF);
                if(flags&(1<<4))
                {
                    p.get(); if(flags&(1<<5)) p.get();
                    if(flags&(1<<6)) loopk(2) p.get();
                }
                if(cp)
                {
					// 220 used to be 180 but that caused a TAG_TYPE disconnect for classes with higher speeds
						//todo: raise (or lower) the bar as more classes are added
					float dvel = max(vel.magnitude2(), (float)fabs(vel.z));
                        if(!ci->local && !m_edit && ((!m_classes && dvel >= 180) || dvel >= 220))
                    	cp->setexceeded();
                    if((!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                    {
                        cp->position.setsize(0);
                        while(curmsg<p.length()) cp->position.add(p.buf[curmsg++]);
                    }
                    if(smode && cp->state.state==CS_ALIVE) smode->moved(cp, cp->state.o, cp->gameclip, pos, (flags&0x80)!=0);
                    cp->state.o = pos;
                    cp->gameclip = (flags&0x80)!=0;
                }
                break;
            }

            case N_TELEPORT:
            {
                int pcn = getint(p), teleport = getint(p), teledest = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                {
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri4x", N_TELEPORT, pcn, teleport, teledest, cp->ownernum);
                }
                break;
            }

            case N_JUMPPAD:
            {
                int pcn = getint(p), jumppad = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                {
                    cp->setpushed();
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri3x", N_JUMPPAD, pcn, jumppad, cp->ownernum);
                }
                break;
            }

            case N_FROMAI:
            {
                int qcn = getint(p);
                if(qcn < 0) cq = ci;
                else
                {
                    cq = getinfo(qcn);
                    if(cq && qcn != sender && cq->ownernum != sender) cq = NULL;
                }
                break;
            }

            case N_EDITMODE:
            {
                int val = getint(p);
                if(!ci->local && !m_edit) break;
                if(val ? ci->state.state!=CS_ALIVE && ci->state.state!=CS_DEAD : ci->state.state!=CS_EDITING) break;
                if(smode)
                {
                    if(val) smode->leavegame(ci);
                    else smode->entergame(ci);
                }
                if(val)
                {
                    ci->state.editstate = ci->state.state;
                    ci->state.state = CS_EDITING;
                    ci->events.setsize(0);
                    ci->state.rockets.reset();
                    ci->state.grenades.reset();
                }
                else ci->state.state = ci->state.editstate;
                QUEUE_MSG;
                break;
            }

            case N_MAPCRC:
            {
                getstring(text, p);
                int crc = getint(p);
                if(!ci) break;
                if(strcmp(text, smapname))
                {
                    if(ci->clientmap[0])
                    {
                        ci->clientmap[0] = '\0';
                        ci->mapcrc = 0;
                    }
                    else if(ci->mapcrc > 0) ci->mapcrc = 0;
                    break;
                }
                copystring(ci->clientmap, text);
                ci->mapcrc = text[0] ? crc : 1;
                checkmaps();
                break;
            }

            case N_CHECKMAPS:
                checkmaps(sender);
                break;

            case N_TRYSPAWN:
                if(!ci || !cq || cq->state.state!=CS_DEAD || cq->state.lastspawn>=0 || (smode && !smode->canspawn(cq))) break;
                if(!ci->clientmap[0] && !ci->mapcrc)
                {
                    ci->mapcrc = -1;
                    checkmaps();
                }
                if(cq->state.lastdeath)
                {
                    flushevents(cq, cq->state.lastdeath + DEATHMILLIS);
                    cq->state.respawn();
                }
                cleartimedevents(cq);
                sendspawn(cq);
                break;

            case N_GUNSELECT:
            {
                int gunselect = getint(p);
                if(!cq || cq->state.state!=CS_ALIVE) break;
                cq->state.gunselect = gunselect;
                QUEUE_AI;
                QUEUE_MSG;
                break;
            }

            case N_SPAWN:
            {
                int ls = getint(p), gunselect = getint(p);
                if(!cq || (cq->state.state!=CS_ALIVE && cq->state.state!=CS_DEAD) || ls!=cq->state.lifesequence || cq->state.lastspawn<0) break;
                cq->state.lastspawn = -1;
                cq->state.state = CS_ALIVE;
                cq->state.gunselect = gunselect;
                cq->exceeded = 0;
                if(smode) smode->spawned(cq);
                QUEUE_AI;
                QUEUE_BUF({
                    putint(cm->messages, N_SPAWN);
                    sendstate(cq->state, cm->messages);
                });
                break;
            }

            case N_SUICIDE:
            {
                if(cq)
                {
					suicideevent *sevent = new suicideevent;
					cq->addevent(sevent);
					sevent->type = getint(p);
                    SbPy::triggerEventInt("player_suicide", cq->clientnum);
                }
                break;
            }

            case N_SHOOT:
            {
                shotevent *shot = new shotevent;
                shot->id = getint(p);
                shot->millis = cq ? cq->geteventmillis(gamemillis, shot->id) : 0;
                shot->gun = getint(p);
                loopk(3) shot->from[k] = getint(p)/DMF;
                loopk(3) shot->to[k] = getint(p)/DMF;
                int hits = getint(p);
                loopk(hits)
                {
                    if(p.overread()) break;
                    hitinfo &hit = shot->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
					hit.headshot = getint(p);
                    loopk(3) hit.dir[k] = getint(p)/DNF;
                }
				shot->numrays = getint(p);
                loopj(shot->numrays)
                {
                    if(p.overread()) break;
                    ivec &ray = shot->rays.add();
                    loopk(3) ray[k] = getint(p);
                }
                if(cq && !cq->state.infected)
                {
                    cq->addevent(shot);
                    cq->setpushed();
                }
                else delete shot;
                break;
            }

            case N_EXPLODE:
            {
                explodeevent *exp = new explodeevent;
                int cmillis = getint(p);
                exp->millis = cq ? cq->geteventmillis(gamemillis, cmillis) : 0;
                exp->gun = getint(p);
                exp->id = getint(p);
				exp->direct = getint(p);
                int hits = getint(p);
                loopk(hits)
                {
                    if(p.overread()) break;
                    hitinfo &hit = exp->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
					hit.headshot = getint(p);
                    loopk(3) hit.dir[k] = getint(p)/DNF;
                }
                if(cq) cq->addevent(exp);
                else delete exp;
                break;
            }

            case N_ITEMPICKUP:
            {
                int n = getint(p);
                if(!cq) break;
                pickupevent *pickup = new pickupevent;
                pickup->ent = n;
                cq->addevent(pickup);
                break;
            }

            case N_TEXT:
            {
                getstring(text, p);
                filtertext(text, text);
                if(SbPy::triggerPolicyEventIntString("allow_message", ci->clientnum, text))
                {  
                   QUEUE_AI;
                   QUEUE_INT(N_TEXT);
                   QUEUE_STR(text);
                   SbPy::triggerEventIntString("player_message", ci->clientnum, text);
                }
                break;
            }

            case N_SAYTEAM:
            {
                getstring(text, p);
                // TODO: Should this event be triggered before or after check?
                if(!ci || !cq || (ci->state.state==CS_SPECTATOR && !ci->local && !ci->privilege) || !m_teammode || !cq->team[0]) break;
                if(SbPy::triggerPolicyEventIntString("allow_message_team", ci->clientnum, text))
                {
                    SbPy::triggerEventIntString("player_message_team", ci->clientnum, text);
                    loopv(clients)
                    {
                        clientinfo *t = clients[i];
                        if(t==cq || t->state.state==CS_SPECTATOR || t->state.aitype != AI_NONE || strcmp(cq->team, t->team)) continue;
                        sendf(t->clientnum, 1, "riis", N_SAYTEAM, cq->clientnum, text);
                    }
                }
                break;
            }

            case N_SWITCHNAME:
            {
                QUEUE_MSG;
                getstring(text, p);
                char *oldname = (char*)malloc(strlen(ci->name)+1);
                strcpy(oldname, ci->name);
                filtertext(ci->name, text, false, MAXNAMELEN);
                if(!ci->name[0]) copystring(ci->name, "unnamed");
                SbPy::triggerEventIntStringString("player_name_changed", ci->clientnum, oldname, ci->name);
                free(oldname);
                QUEUE_STR(ci->name);
                break;
            }

            case N_SWITCHMODEL:
            {
                ci->playermodel = getint(p);
                QUEUE_MSG;
                break;
            }

            case N_SWITCHCLASS:
            {
				ci->state.playerclass = getint(p);
                sendf(-1, 1, "ri3", N_SETCLASS, ci->clientnum, ci->state.playerclass);
                break;
            }

            case N_SWITCHTEAM:
            {
                getstring(text, p);
                filtertext(text, text, false, MAXTEAMLEN);
                SbPy::triggerEventIntString("player_switch_team", sender, text);
                if(strcmp(ci->team, text))
                    sendf(sender, 1, "riis", N_SETTEAM, sender, ci->team);
		/*
                if(strcmp(ci->team, text) && SbPy::triggerPolicyEventIntString("allow_switch_team", ci->clientnum, ci->team))
                {
                    if(m_teammode && smode && !smode->canchangeteam(ci, ci->team, text))
                        sendf(sender, 1, "riis", N_SETTEAM, sender, ci->team);
                    else
                    {
                        if(smode && ci->state.state==CS_ALIVE) smode->changeteam(ci, ci->team, text);
                        copystring(ci->team, text);
                        aiman::changeteam(ci);
                        sendf(-1, 1, "riis", N_SETTEAM, sender, ci->team);
                    }
                    SbPy::triggerEventInt("player_team_changed", ci->clientnum);
                }
		*/
                break;
            }

            case N_MAPVOTE:
            case N_MAPCHANGE:
            {
                getstring(text, p);
                filtertext(text, text, false);
                int reqmode = getint(p);
                if(type==N_MAPVOTE)
                    SbPy::triggerEventIntStringInt("player_map_vote", sender, text, reqmode);
                else
                    SbPy::triggerEventIntStringInt("player_map_set", sender, text, reqmode);
                break;
            }

            case N_ITEMLIST:
            {
                if((ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || !notgotitems || strcmp(ci->clientmap, smapname)) { while(getint(p)>=0 && !p.overread()) getint(p); break; }
                int n;
                while((n = getint(p))>=0 && n<MAXENTS && !p.overread())
                {
                    server_entity se = { NOTUSED, 0, false };
                    while(sents.length()<=n) sents.add(se);
                    sents[n].type = getint(p);
                    if(canspawnitem(sents[n].type))
                    {
                        if(m_mp(gamemode) && delayspawn(sents[n].type)) sents[n].spawntime = spawntime(sents[n].type);
                        else sents[n].spawned = true;
                    }
                }
                notgotitems = false;
                break;
            }

            case N_EDITENT:
            {
                int i = getint(p);
                loopk(3) getint(p);
                int type = getint(p);
                loopk(5) getint(p);
                if(!ci || ci->state.state==CS_SPECTATOR) break;
                QUEUE_MSG;
                bool canspawn = canspawnitem(type);
                if(i<MAXENTS && (sents.inrange(i) || canspawnitem(type)))
                {
                    server_entity se = { NOTUSED, 0, false };
                    while(sents.length()<=i) sents.add(se);
                    sents[i].type = type;
                    if(canspawn ? !sents[i].spawned : (sents[i].spawned || sents[i].spawntime))
                    {
                        sents[i].spawntime = canspawn ? 1 : 0;
                        sents[i].spawned = false;
                    }
                }
                break;
            }

            case N_EDITVAR:
            {
                int type = getint(p);
                getstring(text, p);
                switch(type)
                {
                    case ID_VAR: getint(p); break;
                    case ID_FVAR: getfloat(p); break;
                    case ID_SVAR: getstring(text, p);
                }
                if(ci && ci->state.state!=CS_SPECTATOR) QUEUE_MSG;
                break;
            }

            case N_PING:
                sendf(sender, 1, "i2", N_PONG, getint(p));
                break;

            case N_CLIENTPING:
            {
                int ping = getint(p);
                if(ci)
                {
                    ci->ping = ping;
                    loopv(ci->bots) ci->bots[i]->ping = ping;
                }
                QUEUE_MSG;
                break;
            }

            case N_MASTERMODE:
            {
                int mm = getint(p);
                if(mm>=MM_OPEN && mm<=MM_PRIVATE)
                    SbPy::triggerEventIntInt("player_set_mastermode", ci->clientnum, mm);
                break;
            }

            case N_CLEARBANS:
            {
                SbPy::triggerEventInt("server_clear_bans", ci->clientnum);
                break;
            }

            case N_KICK:
            {
                int victim = getint(p);
                if(getclientinfo(victim)) // no bots
                {
                    SbPy::triggerEventIntInt("player_kick", ci->clientnum, victim);
                    //disconnect_client(victim, DISC_KICK);
                }
                break;
            }

            case N_SPECTATOR:
            {
                int spectator = getint(p), val = getint(p);
               // if(!ci->privilege && !ci->local && (spectator!=sender || (ci->state.state==CS_SPECTATOR && mastermode>=MM_LOCKED))) break;
                clientinfo *spinfo = (clientinfo *)getclientinfo(spectator); // no bots
                if(!spinfo || (spinfo->state.state==CS_SPECTATOR ? val : !val)) break;
                if(val)
                    SbPy::triggerEventIntInt("player_request_spectate", ci->clientnum, spectator);
                else
                    SbPy::triggerEventIntInt("player_request_unspectate", ci->clientnum, spectator);
		//spectate(spinfo, val, spectator);
                break;
            }

            case N_SETTEAM:
            {
                int who = getint(p);
                getstring(text, p);
                filtertext(text, text, false, MAXTEAMLEN);
                //if(!ci->privilege && !ci->local) break;
                clientinfo *wi = getinfo(who);
                if(!wi || !strcmp(wi->team, text)) break;
                if(!smode || smode->canchangeteam(wi, wi->team, text))
                {
                    SbPy::triggerEventIntIntString("player_set_team", ci->clientnum, who, text);
		/*
                    if(smode && wi->state.state==CS_ALIVE)
                        smode->changeteam(wi, wi->team, text);
                    copystring(wi->team, text, MAXTEAMLEN+1);
		*/
                }
                //aiman::changeteam(wi);
                //sendf(-1, 1, "riis", N_SETTEAM, who, wi->team);
                break;
            }

            case N_FORCEINTERMISSION:
                if(ci->local && !hasnonlocalclients()) startintermission();
                break;

            case N_RECORDDEMO:
            {
                int val = getint(p);
                SbPy::triggerEventIntBool("player_record_demo", ci->clientnum, val != 0);
/*
                if(ci->privilege<PRIV_ADMIN && !ci->local) break;
                demonextmatch = val!=0;
                defformatstring(msg)("demo recording is %s for next match", demonextmatch ? "enabled" : "disabled");
                sendservmsg(msg);
*/
                break;
            }

            case N_STOPDEMO:
            {
                if(ci->privilege<PRIV_ADMIN && !ci->local) break;
                stopdemo();
                break;
            }

            case N_CLEARDEMOS:
            {
                int demo = getint(p);
                if(ci->privilege<PRIV_ADMIN && !ci->local) break;
                cleardemos(demo);
                break;
            }

            case N_LISTDEMOS:
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                listdemos(sender);
                break;

            case N_GETDEMO:
            {
                int n = getint(p);
                if(!ci->privilege  && !ci->local && ci->state.state==CS_SPECTATOR) break;
                senddemo(sender, n);
                break;
            }

            case N_GETMAP:
                if(mapdata)
                {
                    sendfile(sender, 2, mapdata, "ri", N_SENDMAP);
                    SbPy::triggerEventInt("player_get_map", ci->clientnum);
		    		ci->connectmillis = totalmillis;
                }
                else sendf(sender, 1, "ris", N_SERVMSG, "no map to send");
                break;

            case N_NEWMAP:
            {
                int size = getint(p);
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                if(size>=0)
                {
                    smapname[0] = '\0';
                    resetitems();
                    notgotitems = false;
                    if(smode) smode->reset(true);
                }
                QUEUE_MSG;
                break;
            }

            case N_SETMASTER:
            {
                int val = getint(p);
                getstring(text, p);
                if(val!=0)
                    SbPy::triggerEventIntString("player_setmaster", ci->clientnum, text);
                else
                    SbPy::triggerEventInt("player_setmaster_off", ci->clientnum);
                // don't broadcast the master password
                break;
            }

            case N_ADDBOT:
            {
                aiman::reqadd(ci, getint(p));
                //Triggered in aiman::addai
                //SbPy::triggerEventInt("game_bot_added", ci->clientnum);
                break;
            }

            case N_DELBOT:
            {
                aiman::reqdel(ci);
                //Triggered in aiman::deleteai
                //SbPy::triggerEventInt("game_bot_removed", ci->clientnum);
                break;
            }

            case N_BOTLIMIT:
            {
                int limit = getint(p);
                if(ci) aiman::setbotlimit(ci, limit);
                break;
            }

            case N_BOTBALANCE:
            {
                int balance = getint(p);
                if(ci) aiman::setbotbalance(ci, balance!=0);
                break;
            }

            case N_AUTHTRY:
            {
                string desc, name;
                getstring(desc, p, sizeof(desc)); // unused for now
                getstring(name, p, sizeof(name));
                if(!desc[0]) tryauth(ci, name);
                break;
            }

            case N_AUTHANS:
            {
                string desc, ans;
                getstring(desc, p, sizeof(desc)); // unused for now
                uint id = (uint)getint(p);
                getstring(ans, p, sizeof(ans));
                if(!desc[0]) answerchallenge(ci, id, ans);
                break;
            }

            case N_PAUSEGAME:
            {
                int val = getint(p);
				SbPy::triggerEventIntBool("player_pause", ci->clientnum, val != 0);
                break;

            }
            case N_COPY:
                ci->cleanclipboard();
                ci->lastclipboard = totalmillis;
                goto genericmsg;

            case N_PASTE:
                if(ci->state.state!=CS_SPECTATOR) sendclipboard(ci);
                goto genericmsg;
    
            case N_CLIPBOARD:
            {
                int unpacklen = getint(p), packlen = getint(p); 
                ci->cleanclipboard(false);
                if(ci->state.state==CS_SPECTATOR)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    break;
                }
                if(packlen <= 0 || packlen > (1<<16) || unpacklen <= 0) 
                {
                    if(packlen > 0) p.subbuf(packlen);
                    packlen = unpacklen = 0;
                }
                packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                putint(q, N_CLIPBOARD);
                putint(q, ci->clientnum);
                putint(q, unpacklen);
                putint(q, packlen); 
                if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                ci->clipboard = q.finalize();
                ci->clipboard->referenceCount++;
                break;
            } 

			case N_ONFIRE:
			{
				int cattacker = getint(p);
				int cvictim = getint(p);
				int gun = getint(p);
				int id = getint(p);
				int on = getint(p);
				clientinfo *ti = getinfo(cattacker);
				clientinfo *vi = getinfo(cvictim);
				if(on && ti && vi && ti->state.state==CS_ALIVE && vi->state.state==CS_ALIVE && (ti==cq || ti->ownernum==cq->clientnum) && vi->state.burnmillis<(gamemillis-200))
				{
					vi->state.onfire = true;
					vi->state.lastburnpain = 0;
					vi->state.burnmillis = gamemillis;
					vi->state.fireattacker = ti;
					sendf(-1, 1, "ri5", N_SETONFIRE, cattacker, cvictim, gun, 1);
				}
				else if (!on && vi && (vi==cq || vi->ownernum==cq->clientnum))
				{
					vi->state.onfire = false;
					sendf(-1, 1, "ri5", N_SETONFIRE, cvictim, cvictim, gun, 0);
				}
				break;
			}

			case N_RADIOALL:
			{
				getstring(text, p);
                filtertext(text, text, false);
				if (text[0] && ci->state.state != CS_SPECTATOR) sendf(-1, 1, "riis", N_RADIOALL, ci->clientnum, text);
				break;
			}

			case N_RADIOTEAM:
			{
				getstring(text, p);
                //filtertext(text, text, false);
				if (text[0] && ci->state.state != CS_SPECTATOR) loopv(clients)
                {
                    clientinfo *t = clients[i];
                    if(t==cq || t->state.state==CS_SPECTATOR || t->state.aitype != AI_NONE || strcmp(cq->team, t->team)) continue;
                    sendf(t->clientnum, 1, "riis", N_RADIOTEAM, cq->clientnum, text);
                }
				break;
			}

			case N_BUY:
			{
				int buyer = getint(p);
				int item = getint(p);
				static const int buyablesprices[game::BA_NUM] = { 500, 900, 500, 900, 600, 1000,
					1000, 1800, 2000, 3600 };
				int guts = buyablesprices[item];
				clientinfo *br = getinfo(buyer);
				if (m_survival && (ci->clientnum==buyer || ci->ownernum==buyer) &&
					br->state.state==CS_ALIVE && br->state.guts>=guts)
				{
					gamestate &gs = ci->state;
					gs.guts -= guts;
					switch (item)
					{
					case game::BA_AMMO:
						{
							const playerclassinfo &pci = playerclasses[gs.playerclass];
							loopi(WEAPONS_PER_CLASS) gs.ammo[pci.weap[i]] = min(gs.ammo[pci.weap[i]] + GUN_AMMO_MAX(pci.weap[i])/2, GUN_AMMO_MAX(pci.weap[i]));
							break;
						}
					case game::BA_AMMOD:
						{
							const playerclassinfo &pci = playerclasses[gs.playerclass];
							loopi(WEAPONS_PER_CLASS) gs.ammo[pci.weap[i]] = GUN_AMMO_MAX(pci.weap[i]);
							break;
						}

					case game::BA_HEALTH:
						gs.health = min(gs.health + gs.maxhealth/2, gs.maxhealth);
						break;
					case game::BA_HEALTHD:
						gs.health = gs.maxhealth;
						break;

					case game::BA_ARMOURG:
					case game::BA_ARMOURY:
						gs.armourtype = (item==game::BA_ARMOURY)? A_YELLOW: A_GREEN;
						gs.armour = (item==game::BA_ARMOURY)? 200: 100;
						break;

					case game::BA_QUAD:
					case game::BA_QUADD:
						gs.quadmillis = (item==game::BA_QUADD)? 60000: 30000;
						break;

					case game::BA_SUPPORT:
					case game::BA_SUPPORTD:
						//spawnsupport((item==game::BA_SUPPORTD)? 6: 3);
						break;
					}
					sendf(-1, 1, "ri4", N_BUY, ci->clientnum, item, guts);
				}
				break;
			}

            #define PARSEMESSAGES 1
            //#include "capture.h"
            #include "ctf.h"
            #include "infection.h"
            #include "survival.h"
            #undef PARSEMESSAGES

            case -1:
                disconnect_client(sender, DISC_TAGT);
                return;

            case -2:
                disconnect_client(sender, DISC_OVERFLOW);
                return;

            default: genericmsg:
            {
                int size = server::msgsizelookup(type);
                if(size<=0) { disconnect_client(sender, DISC_TAGT); return; }
                loopi(size-1) getint(p);
                if(ci && cq && (ci != cq || ci->state.state!=CS_SPECTATOR)) { QUEUE_AI; QUEUE_MSG; }
                break;
            }
        }
    }

    int laninfoport() { return RR_LANINFO_PORT; }
    int serverinfoport(int servport) { return servport < 0 ? RR_SERVINFO_PORT : servport+1; }
    int serverport(int infoport) { return infoport < 0 ? RR_SERVER_PORT : infoport-1; }
    const char *defaultmaster() { return "173.44.35.178"; }
    int masterport() { return RR_MASTER_PORT; }
    int numchannels() { return 3; }

    #include "extinfo.h"

    void serverinforeply(ucharbuf &req, ucharbuf &p)
    {
        if(!getint(req))
        {
            extserverinforeply(req, p);
            return;
        }

        putint(p, numclients(-1, false, true));
        putint(p, 5);                   // number of attrs following
        putint(p, PROTOCOL_VERSION);    // generic attributes, passed back below
        putint(p, gamemode);
        putint(p, max((gamelimit - gamemillis)/1000, 0));
        putint(p, maxclients);
        putint(p, serverpass[0] ? MM_PASSWORD : (!m_mp(gamemode) ? MM_PRIVATE : (mastermode || mastermask&MM_AUTOAPPROVE ? mastermode : MM_AUTH)));
        sendstring(smapname, p);
        sendstring(serverdesc, p);
        sendserverinforeply(p);
    }

    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np)
    {
        return attr.length() && attr[0]==PROTOCOL_VERSION;
    }

    #include "aiman.h"
}