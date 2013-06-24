#ifndef __GAME_H__
#define __GAME_H__

#include "cube.h"

#define TEAM_1 "red"
#define TEAM_2 "blue"

// console message types

enum
{
    CON_CHAT       = 1<<8,
    CON_TEAMCHAT   = 1<<9,
    CON_GAMEINFO   = 1<<10,
    CON_FRAG_SELF  = 1<<11,
    CON_FRAG_OTHER = 1<<12,
    CON_TEAMKILL   = 1<<13
};

// network quantization scale
#define DMF 16.0f                // for world locations
#define DNF 100.0f              // for normalized vectors
#define DVELF 1.0f              // for playerspeed based velocity vectors

enum                            // static entity types
{
    NOTUSED = ET_EMPTY,         // entity slot not in use in map
    LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
    MAPMODEL = ET_MAPMODEL,     // attr1 = idx, attr2 = yaw, attr3 = pitch, attr4 = roll, attr5 = scale
    PLAYERSTART,                // attr1 = angle, attr2 = team
    ENVMAP = ET_ENVMAP,         // attr1 = radius
    PARTICLES = ET_PARTICLES,
    MAPSOUND = ET_SOUND,
    SPOTLIGHT = ET_SPOTLIGHT,   // attr1 = radius, attr2 = yawspeed, attr3 = pitchspeed
	AMMO_L1,AMMO_L2,AMMO_L3,HEALTH_L1,HEALTH_L2,HEALTH_L3,	//once we are using

    TELEPORT,                   // attr1 = idx, attr2 = model, attr3 = tag
    TELEDEST,                   // attr1 = angle, attr2 = idx
    MONSTER,                    // attr1 = angle, attr2 = monstertype
    CARROT,                     // attr1 = tag, attr2 = type
    JUMPPAD,                    // attr1 = zpush, attr2 = ypush, attr3 = xpush
    BASE,
    RESPAWNPOINT,
    BOX,                        // attr1 = angle, attr2 = idx, attr3 = weight
    BARREL,                     // attr1 = angle, attr2 = idx, attr3 = weight, attr4 = health
    PLATFORM,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
    ELEVATOR,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
    FLAG,                       // attr1 = angle, attr2 = team
	TRIGGER,
    MAXENTTYPES,
	I_SHELLS, I_BULLETS, I_ROCKETS, I_ROUNDS, I_GRENADES, I_CARTRIDGES, //remove at some point
	I_HEALTH, I_BOOST,I_GREENARMOUR, I_YELLOWARMOUR,I_QUAD,		//remove at some point
};

struct fpsentity : extentity
{
	void talk(){};
};



enum MonsterStates { M_NONE = 0, M_SEARCH, M_HOME, M_ATTACKING, M_PAIN, M_SLEEP, M_AIMING };  // monster states

enum Modes
{
    M_TEAM       = 1<<0,
    M_NOITEMS    = 1<<1,
    M_NOAMMO     = 1<<2,
    M_CAPTURE    = 1<<3,
    M_CTF        = 1<<4,
    M_OVERTIME   = 1<<5,
    M_EDIT       = 1<<6,
    M_LOCAL      = 1<<7,
    M_LOBBY      = 1<<8,
    M_SURV		 = 1<<9
};

static struct gamemodeinfo
{
    const char *name;
    int flags;
    const char *info;
} gamemodes[] =
{
    { "Survival", M_LOCAL | M_SURV, NULL },
    { "coop edit", M_EDIT, "Cooperative Editing: Edit maps with multiple players simultaneously." },
	{ "TEST LOCAL", M_TEAM | M_EDIT | M_LOCAL, "test mode for non server this is a dev funtion for testing new stuff with out using the server"},
    { "Survival(coop)", M_TEAM, "Fight the zombies, see how long you can survive!" },
    { "capture", M_NOAMMO | M_CAPTURE | M_TEAM, "Capture: Capture neutral bases or steal \fs\f3enemy bases\fr by standing next to them.  \fs\f1Your team\fr scores points for every 10 seconds it holds a base. You spawn with two random weapons and armour. Collect extra ammo that spawns at \fs\f1your bases\fr. There are no ammo items." },
    { "ctf", M_CTF | M_TEAM, "Capture The Flag: Capture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr. Collect items for ammo." },
	{"invasion", M_TEAM, "blah"}
};
#define STARTGAMEMODE (-1)
#define NUMGAMEMODES ((int)(sizeof(gamemodes)/sizeof(gamemodes[0])))

#define m_valid(mode)          ((mode) >= STARTGAMEMODE && (mode) < STARTGAMEMODE + NUMGAMEMODES)
#define m_check(mode, flag)    (m_valid(mode) && gamemodes[(mode) - STARTGAMEMODE].flags&(flag))
#define m_checknot(mode, flag) (m_valid(mode) && !(gamemodes[(mode) - STARTGAMEMODE].flags&(flag)))
#define m_checkall(mode, flag) (m_valid(mode) && (gamemodes[(mode) - STARTGAMEMODE].flags&(flag)) == (flag))

#define m_noitems      (m_check(gamemode, M_NOITEMS))
//(m_check(gamemode, M_NOAMMO|M_NOITEMS))
#define m_lobby			false
#define m_demo			false
#define m_collect		false
#define m_noammo		false
#define m_noweapons		false
#define m_insta			false
#define m_classes		false
#define m_tactics		false
#define m_efficiency	false
#define m_regencapture	false
#define m_protect		false
#define m_hold			false
#define m_infection		false
#define m_teammode		true

#define m_survival     (m_check(gamemode, M_SURVIVAL))
#define m_oneteam      (m_check(gamemode, M_ONETEAM))
#define m_capture      (m_check(gamemode, M_CAPTURE))
#define m_ctf          (m_check(gamemode, M_CTF))
#define m_overtime     (m_check(gamemode, M_OVERTIME))
#define isteam(a,b)    (strcmp(a, b)==0)

#define m_edit         (m_check(gamemode, M_EDIT))
#define m_timed        (m_checknot(gamemode, M_EDIT|M_LOCAL))
#define m_botmode      (m_checknot(gamemode, M_LOCAL))
/**
 * @todo: remove m_mp
 */
#define m_mp(mode)     true //(m_checknot(mode, M_LOCAL))

enum { MM_AUTH = -1, MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE, MM_PASSWORD, MM_START = MM_AUTH };

static const char * const mastermodenames[] =  { "auth",   "open",   "veto",       "locked",     "private",    "password" };
static const char * const mastermodecolors[] = { "",       "\f0",    "\f2",        "\f2",        "\f3",        "\f3" };
static const char * const mastermodeicons[] =  { "server", "server", "serverlock", "serverlock", "serverpriv", "serverpriv" };


// hardcoded sounds, defined in sounds.cfg
enum
{
	// player
    S_JUMP = 0, S_LAND,
    S_PAIN1, S_PAIN2, S_PAIN3,
	S_PAIN4, S_PAIN5, S_PAIN6,
    S_DIE1, S_DIE2,
    S_SPLASH1, S_SPLASH2,
    S_GRUNT1, S_GRUNT2,

	// weapon
	S_SG,
	S_CG,
    S_RLFIRE,
	S_RIFLE,
	S_FLAUNCH,
	S_FLAME,
	S_CBOW,
	S_PISTOL,
	S_PUNCH1,

    S_CHAINSAW_ATTACK,
    S_CHAINSAW_IDLE,

	S_NOAMMO,
	S_RLHIT,
    S_FEXPLODE,
	S_WEAPLOAD,

	S_ICEBALL,
	S_SLIMEBALL,

	// fx
	S_ITEMAMMO, S_ITEMHEALTH, S_ITEMARMOUR,
	S_ITEMPUP, S_ITEMSPAWN, S_TELEPORT,
	S_PUPOUT, S_RUMBLE, S_JUMPPAD,
    S_HIT, S_BURN,
	S_SPLOSH, S_SPLOSH2, S_SPLOSH3,

	// monster
    S_PAINO,
    S_PAINR, S_DEATHR,
    S_PAINE, S_DEATHE,
    S_PAINS, S_DEATHS,
    S_PAINB, S_DEATHB,
    S_PAINP, S_PIGGR2,
    S_PAINH, S_DEATHH,
    S_PAIND, S_DEATHD,
    S_PIGR1,

	// voice
    S_V_BASECAP, S_V_BASELOST,
    S_V_FIGHT,
    S_V_BOOST, S_V_BOOST10,
    S_V_QUAD, S_V_QUAD10,
    S_V_RESPAWNPOINT,
	S_V_ROUNDOVER, S_V_ROUNDDRAW,

	// ctf
    S_FLAGPICKUP,
    S_FLAGDROP,
    S_FLAGRETURN,
    S_FLAGSCORE,
    S_FLAGRESET,
    S_FLAGFAIL, //from tesseract

	// extra
    S_MENU_CLICK
};

// network messages codes, c2s, c2c, s2c

enum AuthorizationTypes { PRIV_NONE = 0, PRIV_MASTER, PRIV_AUTH, PRIV_ADMIN };

enum
{
    N_CONNECT = 0, N_SERVINFO, N_WELCOME, N_INITCLIENT, N_POS, N_TEXT, N_SOUND, N_CDIS,
    N_SHOOT, N_EXPLODE, N_SUICIDE,
    N_DIED, N_DAMAGE, N_HITPUSH, N_SHOTFX, N_EXPLODEFX,
    N_TRYSPAWN, N_SPAWNSTATE, N_SPAWN, N_FORCEDEATH,
    N_GUNSELECT, N_TAUNT,
    N_MAPCHANGE, N_MAPVOTE, N_TEAMINFO, N_ITEMSPAWN, N_ITEMPICKUP, N_ITEMACC, N_TELEPORT, N_JUMPPAD,
    N_PING, N_PONG, N_CLIENTPING,
    N_TIMEUP, N_FORCEINTERMISSION,
    N_SERVMSG, N_ITEMLIST, N_RESUME,
    N_EDITMODE, N_EDITENT, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_REMIP, N_NEWMAP, N_GETMAP, N_SENDMAP, N_CLIPBOARD, N_EDITVAR,
    N_MASTERMODE, N_KICK, N_CLEARBANS, N_CURRENTMASTER, N_SPECTATOR, N_SETMASTER, N_SETTEAM,
    N_BASES, N_BASEINFO, N_BASESCORE, N_REPAMMO, N_BASEREGEN, N_ANNOUNCE,
    N_LISTDEMOS, N_SENDDEMOLIST, N_GETDEMO, N_SENDDEMO,
    N_DEMOPLAYBACK, N_RECORDDEMO, N_STOPDEMO, N_CLEARDEMOS,
    N_TAKEFLAG, N_RETURNFLAG, N_RESETFLAG, N_INVISFLAG, N_TRYDROPFLAG, N_DROPFLAG, N_SCOREFLAG, N_INITFLAGS,
    N_SAYTEAM,
    N_CLIENT,
    N_AUTHTRY, N_AUTHKICK, N_AUTHCHAL, N_AUTHANS, N_REQAUTH,
    N_PAUSEGAME, N_GAMESPEED,
    N_ADDBOT, N_DELBOT, N_INITAI, N_FROMAI, N_BOTLIMIT, N_BOTBALANCE,
    N_MAPCRC, N_CHECKMAPS,
    N_SWITCHNAME, N_SWITCHMODEL, N_SWITCHTEAM, N_SWITCHCLASS, N_SETCLASS,
    N_INITTOKENS, N_TAKETOKEN, N_EXPIRETOKENS, N_DROPTOKENS, N_DEPOSITTOKENS, N_STEALTOKENS,
    N_SERVCMD,
    N_DEMOPACKET,
    NUMMSG
};

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    N_CONNECT, 0, N_SERVINFO, 0, N_WELCOME, 1, N_INITCLIENT, 0, N_POS, 0, N_TEXT, 0, N_SOUND, 2, N_CDIS, 2,
    N_SHOOT, 0, N_EXPLODE, 0, N_SUICIDE, 1,
    N_DIED, 5, N_DAMAGE, 6, N_HITPUSH, 7, N_SHOTFX, 10, N_EXPLODEFX, 4,
    N_TRYSPAWN, 1, N_SPAWNSTATE, 14, N_SPAWN, 3, N_FORCEDEATH, 2,
    N_GUNSELECT, 2, N_TAUNT, 1,
    N_MAPCHANGE, 0, N_MAPVOTE, 0, N_TEAMINFO, 0, N_ITEMSPAWN, 2, N_ITEMPICKUP, 2, N_ITEMACC, 3,
    N_PING, 2, N_PONG, 2, N_CLIENTPING, 2,
    N_TIMEUP, 2, N_FORCEINTERMISSION, 1,
    N_SERVMSG, 0, N_ITEMLIST, 0, N_RESUME, 0,
    N_EDITMODE, 2, N_EDITENT, 11, N_EDITF, 16, N_EDITT, 16, N_EDITM, 16, N_FLIP, 14, N_COPY, 14, N_PASTE, 14, N_ROTATE, 15, N_REPLACE, 17, N_DELCUBE, 14, N_REMIP, 1, N_NEWMAP, 2, N_GETMAP, 1, N_SENDMAP, 0, N_EDITVAR, 0,
    N_MASTERMODE, 2, N_KICK, 0, N_CLEARBANS, 1, N_CURRENTMASTER, 0, N_SPECTATOR, 3, N_SETMASTER, 0, N_SETTEAM, 0,
    N_BASES, 0, N_BASEINFO, 0, N_BASESCORE, 0, N_REPAMMO, 1, N_BASEREGEN, 6, N_ANNOUNCE, 2,
    N_LISTDEMOS, 1, N_SENDDEMOLIST, 0, N_GETDEMO, 2, N_SENDDEMO, 0,
    N_DEMOPLAYBACK, 3, N_RECORDDEMO, 2, N_STOPDEMO, 1, N_CLEARDEMOS, 2,
    N_TAKEFLAG, 3, N_RETURNFLAG, 4, N_RESETFLAG, 6, N_INVISFLAG, 3, N_TRYDROPFLAG, 1, N_DROPFLAG, 7, N_SCOREFLAG, 10, N_INITFLAGS, 0,
    N_SAYTEAM, 0,
    N_CLIENT, 0,
    N_AUTHTRY, 0, N_AUTHKICK, 0, N_AUTHCHAL, 0, N_AUTHANS, 0, N_REQAUTH, 0,
    N_PAUSEGAME, 0, N_GAMESPEED, 0,
    N_ADDBOT, 2, N_DELBOT, 1, N_INITAI, 0, N_FROMAI, 2, N_BOTLIMIT, 2, N_BOTBALANCE, 2,
    N_MAPCRC, 0, N_CHECKMAPS, 1,
    N_SWITCHNAME, 0, N_SWITCHMODEL, 2, N_SWITCHTEAM, 0, N_SWITCHCLASS, 2, N_SETCLASS, 2,
    N_INITTOKENS, 0, N_TAKETOKEN, 2, N_EXPIRETOKENS, 0, N_DROPTOKENS, 0, N_DEPOSITTOKENS, 2, N_STEALTOKENS, 0,
    N_SERVCMD, 0,
    N_DEMOPACKET, 0,
    -1
};

#define RR_SERVER_PORT 28785
#define RR_LANINFO_PORT 28784
#define RR_MASTER_PORT 28787
#define PROTOCOL_VERSION 259            // bump when protocol changes
#define DEMO_VERSION 1                  // bump when demo format changes
#define DEMO_MAGIC "RR_DEMO\0\0"

struct demoheader
{
    char magic[16];
    int version, protocol;
};

#define MAXNAMELEN 15
#define MAXTEAMLEN 4

enum
{
    HICON_BLUE_ARMOUR = 0,
    HICON_GREEN_ARMOUR,
    HICON_YELLOW_ARMOUR,

    HICON_HEALTH,

    HICON_FIST,
    HICON_SG,
    HICON_CG,
    HICON_RL,
    HICON_RIFLE,
    HICON_GL,
    HICON_PISTOL,

    HICON_QUAD,

    HICON_RED_FLAG,
    HICON_BLUE_FLAG,
    HICON_NEUTRAL_FLAG,

    HICON_TOKEN,

    HICON_X       = 20,
    HICON_Y       = 1650,
    HICON_TEXTY   = 1644,
    HICON_STEP    = 490,
    HICON_SIZE    = 120,
    HICON_SPACE   = 40
};

enum GUNS{ GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_RC, GUN_GL, GUN_CARB, GUN_PISTOL,
	GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, GUN_RIFLE, NUMGUNS };


static struct itemstat { float add, sound; const char *name; int icon; int info; } itemstats[] =
{
	{0.205f,S_ITEMAMMO, "AMMO_L1", HICON_CG, 0},
	{0.5f,S_ITEMAMMO, "AMMO_L2", HICON_CG, 0},
	{1.f,S_ITEMAMMO, "AMMO_L3", HICON_CG, 0},
	{0.205f,S_ITEMHEALTH, "HEALTH_L1", HICON_HEALTH, 0},
	{0.5f,S_ITEMHEALTH, "HEALTH_L2", HICON_HEALTH, 0},
	{1.f,S_ITEMHEALTH, "HEALTH_L3", HICON_HEALTH, 0}

};
//static struct itemstat { int add, max, sound; const char *name; int icon, info; } itemstats[] =
//{
//    {10,    30,    S_ITEMAMMO,   "SG", HICON_SG, GUN_SG},
//    {20,    60,    S_ITEMAMMO,   "CG", HICON_CG, GUN_CG},
//    {5,     15,    S_ITEMAMMO,   "RL", HICON_RL, GUN_RL},
//    //{5,     15,    S_ITEMAMMO,   "RI", HICON_RIFLE, GUN_RIFLE},
//    //{10,    30,    S_ITEMAMMO,   "GL", HICON_GL, GUN_GL},
//    //{30,    120,   S_ITEMAMMO,   "PI", HICON_PISTOL, GUN_PISTOL},
//    //{25,    100,   S_ITEMHEALTH, "H", HICON_HEALTH},
//    //{10,    1000,  S_ITEMHEALTH, "MH", HICON_HEALTH},
//    {100,   100,   S_ITEMARMOUR, "GA", HICON_GREEN_ARMOUR},
//    {200,   200,   S_ITEMARMOUR, "YA", HICON_YELLOW_ARMOUR},
//    {20000, 30000, S_ITEMPUP,    "Q", HICON_QUAD},
//};
#define DEBUG 1;
#define MAXRAYS 20 // maxium rays a gun can have
#define EXP_SELFDAM	 1.f //amount of damage absorbed when player hits them self --- damage *= selfdam
#define EXP_SELFPUSH 3.0f //amount of pushback a player gets from hitting themself ---
// next 3 variable form a bell curve, where y = the percentage of damage and x = the distance between attacker and victum
#define DIST_HEIGHT ((40*sqrt(2*PI))) //part of a 3 part bell curve -- this determinds the max height of the bell cuve (y axis) - 40 will keep the number between 100.1004 (hense why it is capped to an int) and 0;
#define DIST_PULL (2*(pow(500.f,2))) // part of a 3 part bell curve -- this determinds the about of pull latterally (x axis) - simply put the higher 500 is the more damage at farther distance: NOTE a bell curve has 2 major parts: is an open curve(closer you get to the middle the less movement), closed curve (the farther you get the less movement) --  look at a bell curve its pretty self explanitory -- this number controls the closed curve
#define DIST_PUSH 50.f //part of a 3 part bell curve -- this determinds the bottom curve (open curve) of the bell curve - simply put the this pulls the middle (where the two curve meet) forward (towards 0 along the x) the higher this number the farther out the middle is -- do not modify this number unless you know what you are doint (this is basically the mean )
#define EXP_DISTSCALE 3.0f//the power at which the explosion distance is calculated on -- damage*(1/scale * (scale ^ (1- (dist/maxdist))))

static const struct guninfo { int sound, attackdelay, damage, maxammo, spread, projspeed, kickamount, range, rays, hitpush, exprad, ttl; const char *name, *file; short part; int icon; short reload; int rewait; } guns[NUMGUNS] =
{
	//sound		  attd	damage ammo	sprd	prsd	kb	rng	 rays  htp	exr	 ttl  name					file			part        icon        reload rewait
    { S_PUNCH1,    250,  50,	1,	0,		  0,	0,   14,   1,   80,	 0,    0, "MELEE",				"",					 0,     HICON_FIST,    1,       0},
    { S_SG,       1400,  10,	36,	400,	  0,	20, 1024, 20,   80,  0,    0, "shotgun",			"",					 0,     HICON_SG,    4,     2000},
    { S_CG,        100,  30,	200,100,	  0,	7,  1024,  1,   80,  0,    0, "machine gun",        "",					 0,     HICON_CG, 	40,		2000},
    { S_RLFIRE,    800, 120,	10,	0,		320,	10, 1024,  1,  160, 40,    0, "RPG",				"",					 0,     HICON_RIFLE, 4,		1500},
    { S_FLAUNCH,   600,  90,	20,	0,		200,	10, 1024,  1,  250, 45, 1500, "Razzor Cannon",		"",					 0,     HICON_RL,	4,		1200}, // razzor cannon
	{ S_FLAUNCH,   600,  90,	20,	0,		200,	10, 1024,  1,  250, 45, 1500, "grenadelauncher",	"",					 0,     HICON_RL,	8,		500},
	{ S_PISTOL,    500,  35,	60,	50,		  0,	 7,	1024,  1,   80,  0,    0, "carbine",			"",					 0,     HICON_PISTOL,	16,	700}, //carbine
    { S_PISTOL,    500,  35,	60,	50,		  0,	 7,	1024,  1,   80,  0,    0, "revolver",			"",		 			 0,     HICON_PISTOL,	7,	200},
    { S_FLAUNCH,   200,  20,	0,	0,		200,	 1,	1024,  1,   80, 40,    0, "fireball",			NULL,	PART_FIREBALL1,     HICON_RL,		1,	0},
    { S_ICEBALL,   200,  40,	0,	0,		120,	 1,	1024,  1,   80, 40,    0, "iceball",			NULL,   PART_FIREBALL2,     HICON_FIST,		1,	0 },
    { S_SLIMEBALL, 200,  30,	0,	0,		640,	 1,	1024,  1,   80, 40,    0, "slimeball",			NULL,   PART_FIREBALL3,     HICON_FIST,		1,	0 },
    { S_PIGR1,     250,  50,	0,	0,		  0,	 1,	  12,  1,   80,  0,    0, "bite",				NULL,				 0,     HICON_FIST,		1,	0 },
    { -1,            0, 120,	0,	0,		  0,	 0,    0,  1,   80, 40,    0, "barrel",				NULL,				 0,     HICON_FIST,		1,	0 },
	{-1,			 0,	  0,	0,	0,		  0,	 0,	   0,  0,	 0,	 0,	   0,  "",					"",					 0,     HICON_FIST,		1,	0 }
};

#define GUN_AMMO_MAX(gunselect) guns[gunselect].maxammo
#define NUMWEAPS NUMGUNS
#define WEAP_USABLE(weapon) true

//static char *PmodelDir[6]= {"","","","","",""};

enum PlayerClassID
{
	PCS_PREP,
	PCS_MOTER,
	PCS_SWAT,
	PCS_SCI,
	PCS_ADVENT,
	PCS_BLANK,
	NUMPCS
};



static const struct PlayerClass{ const char *name; int modelId, speed, maxhealth,  guns[3], utility;} PClasses[6] =
{
	//name		MId	Speed	health	guns		utility;
	{"Preper",	0,	55,		150,	{GUN_RL,	GUN_CARB,	0},		0	},
	{"Moter",	1,	53,		175,	{GUN_RC,	GUN_GL,		0},		0	},
	{"S.W.A.T.",2,	45,		250,	{GUN_SG,	GUN_PISTOL,	0},		0	},
	{"Solider",	3,	50,		200,	{GUN_CG,	GUN_PISTOL,	0},		0	},
	{"Sciencist",4,	60,		160,	{0,			0,			0},		0	},
	{"Adventure",5,	65,		125,	{0,			0,			0},		0	}
};

#define WEAPONS_PER_CLASS 3
static char *classinfo[NUMPCS] = { NULL, NULL, NULL, NULL, NULL };

const char *getclassinfo(int c);
//killme: moved commands to client.cpp


#include "ai.h"

// inherited by fpsent and server clients
struct fpsstate
{
    int health, maxhealth;
    int armour, armourtype;
    int quadmillis;
    int gunselect, gunwait;
    int ammo[NUMGUNS];
	int reload[NUMGUNS];
	int reloadwait;
    int aitype, skill;
	int pclass; //curent playerclass
	PlayerClass pcs;
	bool sprinting;

    //Fire
    bool onfire;
    int burnmillis;

    fpsstate() : maxhealth(100), aitype(AI_NONE), skill(0), pclass(0), onfire(false) {}

    void baseammo(int gun, int k = 2, int scale = 1)
    {
		guninfo gs = guns[gun];
        ammo[gun] = gs.maxammo; // (itemstats[gun-GUN_SG].add*k)/scale;
    }

    void addammo(int gun, int k = 1, int scale = 1)
    {
        itemstat &is = itemstats[gun];
		const PlayerClass &pcs = PClasses[pclass];
		loopi(WEAPONS_PER_CLASS){
			const guninfo &gi = guns[pcs.guns[i]];
			const int a = pcs.guns[i];
			ammo[a] = min((int((is.add)*gi.maxammo)+ammo[pcs.guns[i]]),gi.maxammo);
		}

    }

    bool hasmaxammo()
    {
		const PlayerClass &pcs = PClasses[pclass];
		loopi(WEAPONS_PER_CLASS){
			const guninfo &gi = guns[pcs.guns[i]];
			if(gi.maxammo != ammo[pcs.guns[i]])
				return false;
		}
		return true;
    }

    bool canpickup(int type)
    {
		if(type<AMMO_L1 || type>HEALTH_L3) return false;
        //itemstat &is = itemstats[type-I_SHELLS];
        switch(type)
        {
			case HEALTH_L1:
			case HEALTH_L2:
            case HEALTH_L3: return health<maxhealth;
			case AMMO_L1:
			case AMMO_L2:
			case AMMO_L3: return !hasmaxammo();
			default: return false;
        }
    }

    void pickup(int type)
    {
        if(type<AMMO_L1 || type>HEALTH_L3) return;
        itemstat &is = itemstats[type-AMMO_L1];
        switch(type)
		{
			case HEALTH_L1:
			case HEALTH_L2:
            case HEALTH_L3:
                health = min(int((maxhealth*(is.add))+health), maxhealth);
                break;
            default:
                addammo(type-AMMO_L1);
                break;
        }
    }

    void respawn()
    {
		//if(npclass >-1 && npclass < NUMPCS) pclass = npclass; npclass = -1;
		pcs = PClasses[pclass];
		maxhealth = health = pcs.maxhealth;
        armour = 0;
        quadmillis = 0;
		gunselect = pcs.guns[0];
        gunwait = 0;
		reloadwait = 0;
        loopi(NUMGUNS) ammo[i] = 0;
		loopi(NUMGUNS) reload[i] = 0;
		loopi(WEAPONS_PER_CLASS-1){
		const guninfo &gi = guns[pcs.guns[i]];
		ammo[pcs.guns[i]] = gi.maxammo;
		reload[pcs.guns[i]] = gi.reload;
		}
		ammo[pcs.guns[WEAPONS_PER_CLASS-1]] = 1;
    }

    void spawnstate(int gamemode){}

    // just subtract damage here, can set death, etc. later in code calling this
    int dodamage(int damage)
    {
        health -= damage;
        return damage;
    }

    int hasammo(int gun, int exclude = -1)
    {
        return gun >= 0 && gun <= NUMGUNS && gun != exclude && ammo[gun] > 0;
    }
	void reloaded()
	{
		if(reloadwait) return;
		int amountleft = reload[gunselect];
		const guninfo &gi = guns[gunselect];
		if(ammo[gunselect] < gi.reload) reload[gunselect] = ammo[gunselect];
		else reload[gunselect] = gi.reload;
		ammo[gunselect] -= (gi.reload-amountleft);
		if(ammo[gunselect] < 0) ammo[gunselect] = 0;
	}
	void startreload()
	{
		if(reloadwait) return;
		const guninfo &gi = guns[gunselect];
		if (reload[gunselect] == gi.reload) return;
		reloadwait = gi.rewait+lastmillis+gunwait;
	}
};

struct fpsent : dynent, fpsstate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain;
    int lastaction, lastattackgun;
    bool attacking;
    int attacksound, attackchan, idlesound, idlechan;
    int lasttaunt;
    int lastpickup, lastpickupmillis, lastbase, lastrepammo, flagpickup, tokens;
    vec lastcollect;
    int frags, flags, deaths, totaldamage, totalshots;
    editinfo *edit;
    float deltayaw, deltapitch, deltaroll, newyaw, newpitch, newroll;
    int smoothmillis;

    string name, team, info;
    int playermodel;
    ai::aiinfo *ai;
    int ownernum, lastnode;

    vec muzzle;

    fpsent() : weight(100), clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0), lifesequence(0), respawned(-1), suicided(-1), lastpain(0), attacksound(-1), attackchan(-1), idlesound(-1), idlechan(-1), frags(0), flags(0), deaths(0), totaldamage(0), totalshots(0), edit(NULL), smoothmillis(-1), playermodel(-1), ai(NULL), ownernum(-1), muzzle(-1, -1, -1)
    {
        name[0] = team[0] = info[0] = 0;
        respawn();
    }
    ~fpsent()
    {
        freeeditinfo(edit);
        if(attackchan >= 0) stopsound(attacksound, attackchan);
        if(idlechan >= 0) stopsound(idlesound, idlechan);
        if(ai) delete ai;
    }

    void hitpush(int damage, const vec &dir, fpsent *actor, int gun)
    {
        vec push(dir);
        push.mul((actor==this && guns[gun].exprad ? EXP_SELFPUSH : 1.0f)*guns[gun].hitpush*damage/weight);
        vel.add(push);
    }

    void stopattacksound()
    {
        if(attackchan >= 0) stopsound(attacksound, attackchan, 250);
        attacksound = attackchan = -1;
    }

    void stopidlesound()
    {
        if(idlechan >= 0) stopsound(idlesound, idlechan, 100);
        idlesound = idlechan = -1;
    }

    void respawn()
    {
		if(pclass <0 || pclass >= NUMPCS){ showgui("playerclass"); return;}
        dynent::reset();
        fpsstate::respawn();
        respawned = suicided = -1;
        lastaction = 0;
        lastattackgun = gunselect;
        attacking = false;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        lastbase = lastrepammo = -1;
        flagpickup = 0;
        tokens = 0;
        lastcollect = vec(-1e10f, -1e10f, -1e10f);
        stopattacksound();
        lastnode = -1;
		const PlayerClass &pcs = PClasses[pclass];
		maxspeed = pcs.speed;
    }
};

struct teamscore
{
    const char *team;
    int score;
    teamscore() {}
    teamscore(const char *s, int n) : team(s), score(n) {}

    static bool compare(const teamscore &x, const teamscore &y)
    {
        if(x.score > y.score) return true;
        if(x.score < y.score) return false;
        return strcmp(x.team, y.team) < 0;
    }
};

static inline uint hthash(const teamscore &t) { return hthash(t.team); }
static inline bool htcmp(const char *key, const teamscore &t) { return htcmp(key, t.team); }

#define MAXTEAMS 128

struct teaminfo
{
    char team[MAXTEAMLEN+1];
    int frags;
};

static inline uint hthash(const teaminfo &t) { return hthash(t.team); }
static inline bool htcmp(const char *team, const teaminfo &t) { return !strcmp(team, t.team); }

namespace entities
{
    extern vector<extentity *> ents;

    extern const char *entmdlname(int type);
    extern const char *itemname(int i);
    extern int itemicon(int i);

    extern void preloadentities();
    extern void renderentities();
    extern void checkitems(fpsent *d);
    extern void checkquad(int time, fpsent *d);
    extern void resetspawns();
    extern void spawnitems(bool force = false);
    extern void putitems(packetbuf &p);
    extern void setspawn(int i, bool on);
    extern void teleport(int n, fpsent *d);
    extern void pickupeffects(int n, fpsent *d);
    extern void teleporteffects(fpsent *d, int tp, int td, bool local = true);
    extern void jumppadeffects(fpsent *d, int jp, bool local = true);

    extern void repammo(fpsent *d, int type, bool local = true);
}

namespace game
{
    struct clientmode
    {
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual int clipconsole(int w, int h) { return 0; }
        virtual void drawhud(fpsent *d, int w, int h) {}
        virtual void rendergame() {}
        virtual void respawned(fpsent *d) {}
        virtual void setup() {}
        virtual void checkitems(fpsent *d) {}
        virtual int respawnwait(fpsent *d) { return 0; }
        virtual void pickspawn(fpsent *d) { findplayerspawn(d); }
        virtual void senditems(packetbuf &p) {}
        virtual void removeplayer(fpsent *d) {}
        virtual void gameover() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(const char *team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual void aifind(fpsent *d, ai::aistate &b, vector<ai::interest> &interests) {}
        virtual bool aicheck(fpsent *d, ai::aistate &b) { return false; }
        virtual bool aidefend(fpsent *d, ai::aistate &b) { return false; }
        virtual bool aipursue(fpsent *d, ai::aistate &b) { return false; }
    };

    extern clientmode *cmode;
    extern void setclientmode();

    // fps
    extern int gamemode, nextmode;
    extern string clientmap;
    extern bool intermission;
    extern int maptime, maprealtime, maplimit;
    extern fpsent *player1;
    extern vector<fpsent *> players, clients;
    extern int lastspawnattempt;
    extern int lasthit;
    extern int respawnent;
    extern int following;
    extern int smoothmove, smoothdist;

    extern bool clientoption(const char *arg);
    extern fpsent *getclient(int cn);
    extern fpsent *newclient(int cn);
    extern const char *colorname(fpsent *d, const char *name = NULL, const char *prefix = "", const char *suffix = "", const char *alt = NULL);
    extern const char *teamcolorname(fpsent *d, const char *alt = "you");
    extern const char *teamcolor(const char *name, bool sameteam, const char *alt = NULL);
    extern const char *teamcolor(const char *name, const char *team, const char *alt = NULL);
    extern fpsent *pointatplayer();
    extern fpsent *hudplayer();
    extern fpsent *followingplayer();
    extern void stopfollowing();
    extern void clientdisconnected(int cn, bool notify = true);
    extern void clearclients(bool notify = true);
    extern void startgame();
    extern void spawnplayer(fpsent *);
    extern void deathstate(fpsent *d, bool restore = false);
    extern void damaged(int damage, fpsent *d, fpsent *actor, bool local = true);
    extern void killed(fpsent *d, fpsent *actor);
    extern void timeupdate(int timeremain);
    extern void msgsound(int n, physent *d = NULL);
    extern void drawicon(int icon, float x, float y, float sz = 120);
    const char *mastermodecolor(int n, const char *unknown);
    const char *mastermodeicon(int n, const char *unknown);

    // client
    extern bool connected, remote, demoplayback;
    extern string servinfo;

    extern int parseplayer(const char *arg);
    extern void ignore(int cn);
    extern void unignore(int cn);
    extern bool isignored(int cn);
    extern void addmsg(int type, const char *fmt = NULL, ...);
    extern void switchname(const char *name);
    extern void switchteam(const char *name);
	extern void switchplayerclass(int playerclass);
    extern void switchplayermodel(int playermodel);
    extern void sendmapinfo();
    extern void stopdemo();
    extern void changemap(const char *name, int mode);
    extern void c2sinfo(bool force = false);
    extern void sendposition(fpsent *d, bool reliable = false);
	extern void togglespectator(int val, const char *who);


    // weapon
    extern int getweapon(const char *name);
    extern void shoot(fpsent *d, const vec &targ);
    extern void shoteffects(int gun, const vec &from, const vec &to, fpsent *d, bool local, int id, int prevaction);
    extern void explode(bool local, fpsent *owner, const vec &v, dynent *safe, int dam, int gun);
    extern void explodeeffects(int gun, fpsent *d, bool local, int id = 0);
    extern void damageeffect(int damage, fpsent *d, bool thirdperson = true);
    extern void gibeffect(int damage, const vec &vel, fpsent *d);
    extern float intersectdist;
    extern bool intersect(dynent *d, const vec &from, const vec &to, float &dist = intersectdist);
    extern dynent *intersectclosest(const vec &from, const vec &to, fpsent *at, float &dist = intersectdist);
    extern void clearbouncers();
    extern void updatebouncers(int curtime);
    extern void removebouncers(fpsent *owner);
    extern void renderbouncers();
    extern void clearprojectiles();
    extern void updateprojectiles(int curtime);
    extern void removeprojectiles(fpsent *owner);
    extern void renderprojectiles();
    extern void preloadbouncers();
    extern void removeweapons(fpsent *owner);
    extern void updateweapons(int curtime);
    extern void gunselect(int gun, fpsent *d);
    extern void weaponswitch(fpsent *d);
    extern void avoidweapons(ai::avoidset &obstacles, float radius);

    // scoreboard
    extern void showscores(bool on);
    extern void getbestplayers(vector<fpsent *> &best);
    extern void getbestteams(vector<const char *> &best);
    extern void clearteaminfo();
    extern void setteaminfo(const char *team, int frags);
    extern int groupplayers(); //killme: made public
    struct scoregroup : teamscore //killme: made public
    {
        vector<fpsent *> players;
    };
    extern vector<scoregroup *> groups; //killme: made public

    // render
    struct playermodelinfo
    {
        const char *ffa, *blueteam, *redteam, *hudguns,
                   *vwep, *quad, *armour[3],
                   *ffaicon, *blueicon, *redicon;
        bool ragdoll;
    };

    extern int playermodel, teamskins, testteam;

    extern void saveragdoll(fpsent *d);
    extern void clearragdolls();
    extern void moveragdolls();
    extern void changedplayermodel();
    extern const playermodelinfo &getplayermodelinfo(fpsent *d);
    extern int chooserandomplayermodel(int seed);
    extern void swayhudgun(int curtime);
    extern vec hudgunorigin(int gun, const vec &from, const vec &to, fpsent *d);
}

namespace server
{
    extern const char *modename(int n, const char *unknown = "unknown");
    extern const char *mastermodename(int n, const char *unknown = "unknown");
    extern void startintermission();
    extern void stopdemo();
    extern void forcemap(const char *map, int mode);
    extern void forcepaused(bool paused);
    extern void forcegamespeed(int speed);
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
    extern int msgsizelookup(int msg);
    extern bool serveroption(const char *arg);
    extern bool delayspawn(int type);
}

#endif



