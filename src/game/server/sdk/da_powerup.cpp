#include "cbase.h"
#include "sdk_player.h"

ConVar  da_powerup_health ("da_powerup_health", "100", FCVAR_NOTIFY|FCVAR_REPLICATED);
ConVar  da_powerup_style ("da_powerup_style", "1", FCVAR_NOTIFY|FCVAR_REPLICATED);
ConVar  da_powerup_magazine ("da_powerup_magazine", "3", FCVAR_NOTIFY|FCVAR_REPLICATED);
ConVar  da_powerup_slowmo ("da_powerup_slowmo", "5", FCVAR_NOTIFY|FCVAR_REPLICATED);
enum powerup_e
{
	POWERUP_HEALTH = 0,
	POWERUP_STYLEFILL,
	POWERUP_MAGAZINE,
	POWERUP_SLOWMO,
	POWERUP_GRENADE,
	MAX_POWERUP
};
static const char *models[] =
{/*Must be given in the same order as powerup_e*/
	"models/gibs/airboat_broken_engine.mdl",
	"models/gibs/airboat_broken_engine.mdl",
	"models/gibs/airboat_broken_engine.mdl",
	"models/gibs/airboat_broken_engine.mdl",
	"models/gibs/airboat_broken_engine.mdl",
};
class Powerup : public CBaseAnimating
{
public:
	DECLARE_CLASS(Powerup, CBaseAnimating);
	DECLARE_DATADESC();

	void Spawn (void);
	void Precache (void);
	void pickup (CBaseEntity *other);
	void manifest (void);
public:
	float mindelay, maxdelay;
	unsigned short index;
	unsigned char numtypes;
	unsigned char types[MAX_POWERUP];
};
LINK_ENTITY_TO_CLASS(da_powerup, Powerup);
BEGIN_DATADESC(Powerup)
	DEFINE_KEYFIELD(mindelay, FIELD_FLOAT, "mindelay"),
	DEFINE_KEYFIELD(maxdelay, FIELD_FLOAT, "maxdelay"),
	DEFINE_ENTITYFUNC(pickup),
	DEFINE_THINKFUNC(manifest),
END_DATADESC()

void 
Powerup::Precache (void)
{
	numtypes = 0;
	if (!HasSpawnFlags (~0))
	{/*Randomly select a powerup to spawn for the duration of the game*/
		types[numtypes++] = RandomInt (0, MAX_POWERUP-1);
		PrecacheModel (models[types[0]]);
	}
	else
	{/*Precache everything set in the bitmask*/
		int i;
		for (i = 0; i < MAX_POWERUP; i++)
		{
			if (HasSpawnFlags (1<<i))
			{
				PrecacheModel (models[i]);
				types[numtypes++] = i;
			}
		}
	}
}
void 
Powerup::Spawn (void)
{
	Precache ();
	manifest ();
}
void
Powerup::manifest (void)
{
	index = RandomInt (0, numtypes-1);
	SetModel (models[types[index]]);
	RemoveEffects (EF_NODRAW);
	SetSolid (SOLID_BBOX);
	AddSolidFlags (FSOLID_TRIGGER|FSOLID_NOT_SOLID);
	SetMoveType (MOVETYPE_NONE);
	UTIL_SetSize (this, -Vector (32,32,32), Vector (32,32,32));

	SetThink (NULL);
	SetTouch (&Powerup::pickup);

	CPASFilter filter (GetAbsOrigin ());
	filter.UsePredictionRules ();
	EmitSound (filter, entindex (), "Item.Materialize");
}
void
Powerup::pickup (CBaseEntity *other)
{
	const int MAXENTS = 64;
	CSDKPlayer *ents[MAXENTS];
	CSDKPlayer *taker;
	Vector pos;
	int denied;
	int len;
	int i;

	if (!other->IsPlayer ())
	{
		return;
	}
	/*Everyone is cool but also, somewhat parodoxically, of low selfesteem.
	So don't rub it in their face that they were denied.
	Still reward the taker though.*/
	denied = 0;
	taker = (CSDKPlayer *)other;
	VectorCopy (GetAbsOrigin (), pos);
	len = UTIL_EntitiesInSphere ((CBaseEntity **)ents, 
								 MAXENTS, 
								 pos, 
								 128, 
								 FL_CLIENT);
	for (i = 0; i < len; i++)
	{
		CSDKPlayer *loser = ents[i];
		if (loser != taker) 
		{/*Ensure loser was looking at the powerup*/
			Vector org, forward;
			AngleVectors (loser->EyeAngles (), &forward);
			VectorSubtract (loser->GetAbsOrigin (), pos, org);
			VectorNormalize (org);
			if (DotProduct (forward, org) < 0)
			{
				denied++;
				break;
			}
		}
	}
	if (len != 1) 
	{/*I don't know what's happening here, why is this so complicated?*/
		taker->AddStylePoints (3*denied, STYLE_POINT_STYLISH);
		taker->SendAnnouncement (ANNOUNCEMENT_COOL, STYLE_POINT_STYLISH);
	}
	CPASFilter filter (pos);
	filter.UsePredictionRules ();
	EmitSound (filter, entindex (), "HealthVial.Touch");
	/*Impart pickup bonus*/
	switch (types[index])
	{
	case POWERUP_HEALTH: 
		taker->TakeHealth (da_powerup_health.GetFloat (), DMG_GENERIC);
		break;
	case POWERUP_STYLEFILL:
	{
		ConVarRef activation ("dab_stylemeteractivationcost");
		float pts = 1+da_powerup_style.GetFloat ()*activation.GetFloat ();
		taker->AddStylePoints (pts, STYLE_POINT_STYLISH);
		break;
	}
	case POWERUP_MAGAZINE:
	{
		int maxclip = taker->GetActiveSDKWeapon ()->GetWpnData ().iMaxClip1;
		int bonus = da_powerup_magazine.GetInt ()*maxclip;
		taker->GetActiveSDKWeapon ()->m_iClip1 = bonus;
		break;
	}
	case POWERUP_SLOWMO: 
		taker->GiveSlowMo (da_powerup_slowmo.GetFloat ()); 
		break;
	case POWERUP_GRENADE:
		taker->GiveNamedItem ("weapon_grenade");
		break;
	default: /*Shouldn't get here*/ break;
	}
	/*Go inactive until respawn timer expires*/
	AddEffects (EF_NODRAW);
	AddSolidFlags (FSOLID_NOT_SOLID);
	SetTouch (NULL);
	SetThink (&Powerup::manifest);
	SetNextThink (gpGlobals->curtime + RandomFloat (mindelay, maxdelay));
}
#ifdef DEBUG
void powerup(const CCommand &args)
{
	CBasePlayer *player = ToBasePlayer (UTIL_GetCommandClient ()); 
	if (!player)
	{
		return;
	}
	Vector org;
	Vector forward;

	AngleVectors (player->EyeAngles (), &forward);
	VectorMA (player->GetAbsOrigin () + Vector (0, 0, 36), 80, forward, org);

	Powerup *p = (Powerup *)CreateEntityByName ("da_powerup");
	p->AddSpawnFlags (~0);
	p->mindelay = 5;
	p->maxdelay = 10;
	p->Spawn ();
	p->SetAbsOrigin (org);
}
static ConCommand test_powerup ("test_powerup", powerup);
#endif