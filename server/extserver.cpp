
/**
 * Lua server api
 * @todo add more apis
 */
namespace server
{
    extern clientinfo *getinfo(int n);

    namespace lua
    {
       void sendPlayerMsg(int cn,const char * text)
        {
            clientinfo *ci = getinfo(cn);
            //FIX
        }

        int getMinutesLeft()
        {
            return (gamemillis>=gamelimit ? 0 : (gamelimit - gamemillis + 60000 - 1)/60000);
        }

        void setMinutesLeft(int mins)
        {

            //changetime(mins * 1000 * 60);
        }

        int getSecondsLeft()
        {
            return (gamemillis>=gamelimit ? 0 : (gamelimit - gamemillis) / 1000);
        }

        void setSecondsLeft(int seconds)
        {
            //changetime(seconds * 1000);
        }

        int getPlayerSessionId(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->sessionid : NULL;
        }

        const char *getPlayerName(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->name : NULL;
        }

        void setPlayerName(int cn, const char *name)
        {
            string safenewname;
            filtertext(safenewname, name, false, MAXNAMELEN);
            if(!safenewname[0]) copystring(safenewname, "unnamed");

            clientinfo *ci = getinfo(cn);
            if(!ci) return;

            putuint(ci->messages, N_SWITCHNAME);
            sendstring(safenewname, ci->messages);

            copystring(ci->name, safenewname, MAXNAMELEN+1);
        }

        const char *getDisplayName(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? colorname(ci) : NULL;
        }

        const char *getTeam(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->team : NULL;
        }

        uint getIp(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? getclientip(ci->clientnum) : NULL;
        }

        int getStatusCode(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.state : -1;
        }

        int getEditStatusCode(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.editstate : -1;
        }

        int getPing(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->ping : -1;
        }

        int getLastDeath(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.lastdeath : -1;
        }

        int getPlayerFrags(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.frags : 0;
        }

        int getTeamkills(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.teamkills : 0;
        }

        int getDeaths(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.deaths : 0;
        }

        int getDamage(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.damage : 0;
        }

        int getShotDamage(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.shotdamage : 0;
        }

        int getMaxHealth(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.maxhealth : 0;
        }

        /**
         * Sets the health capacity of the player specified by the cn.
         *
         * @todo Send update message to clients.
         * @param cn
         * @param maxHealth
         */
        void setMaxHealth(int cn, int maxHealth)
        {
            clientinfo *ci = getinfo(cn);
            if(!ci) return;
            ci->state.maxhealth = maxHealth;
        }

        int getHealth(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.health : 0;
        }

        void setHealth(int cn, int health)
        {
            clientinfo *ci = getinfo(cn);
            if(!ci) return;
            ci->state.health = health;
        }

        int getArmour(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.armour : -1;
        }

        void setArmour(int cn, int armour)
        {
            clientinfo *ci = getinfo(cn);
            if(!ci) return;
            ci->state.armour = armour;
        }

        int getArmourType(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.armourtype : -1;
        }

        void setArmourType(int cn, int armourType)
        {
            clientinfo *ci = getinfo(cn);
            if(!ci) return;
            ci->state.armourtype = armourType;
        }

        int getGunSelect(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.gunselect : -1;
        }

        int getPrivilege(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->privilege : -1;
        }

        int getConnectionTime(int cn)
        {
            clientinfo *ci = getinfo(cn);
            if (!ci) return -1;
            return totalmillis - ci->connectmillis;
        }

        int getTimePlayed(int cn)
        {
            clientinfo *ci = getinfo(cn);
            if (!ci) return -1;
            return ci->state.timeplayed + (ci->state.state != CS_SPECTATOR ? (lastmillis - ci->state.lasttimeplayed) : 0);
        }

        void slayPlayer(int cn)
        {
            clientinfo * ci = getinfo(cn);
            if(!ci || ci->state.state != CS_ALIVE) return;

            ci->state.state = CS_DEAD;
            sendf(-1, 1, "ri2", N_FORCEDEATH, cn);
        }

        void setTeam(int cn, const char *team)
        {
            clientinfo *ci = getinfo(cn);

            if(!ci) return;

            string newteam;
            copystring(ci->team, newteam, MAXTEAMLEN+1);
            sendf(-1, 1, "riis", N_SETTEAM, cn, newteam);

            if(ci->state.aitype == AI_NONE) aiman::dorefresh = true;
        }

        bool isBot(int cn)
        {
            clientinfo *ci = getinfo(cn);
            return ci ? ci->state.aitype != AI_NONE : false;
        }

        void changeMap(const char * map, int mode)
        {
            sendf(-1, 1, "risii", N_MAPCHANGE, map, mode, 1);
            changemap(map, mode);
        }

        int getClientCount(int exclude = -1, bool nospec = true, bool noai = true, bool priv = false)
        {
            return numclients(exclude, nospec, noai, priv);
        }

        void setSpectator(int cn, bool spectator = true)
        {
            clientinfo *ci = getinfo(cn);
            if(!ci || ci->state.aitype != AI_NONE) return;
            ci->state.state = CS_SPECTATOR;
            sendf(-1, 1, "ri3", N_SPECTATOR, ci->clientnum, spectator ? 1: 0);
        }

        /**
         * Sets the prvivilege of the client specified by the cn.
         * showPublic specifies if the other clients can see the privilege.
         *
         * @unimplemented
         * @todo: use a function in server.cpp
         * @param cn
         * @param code
         * @param showPublic
         */
        void setPrvivilege(int cn, int code, bool showPublic = true)
        {
            clientinfo *ci = getinfo(cn);
            if(!ci || ci->state.aitype != AI_NONE || ci->privilege == code) return;

        }

        void setFrozen(int cn, bool frozen = true)
        {
            clientinfo *ci = getinfo(cn);
            if(!ci) return;
            sendf(ci->clientnum, 1, "rii", N_PAUSEGAME, frozen ? 1 : 0);
        }
    }
}
