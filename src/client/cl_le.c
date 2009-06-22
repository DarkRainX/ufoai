/**
 * @file cl_le.c
 * @brief Local entity management.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "cl_le.h"
#include "sound/s_main.h"
#include "sound/s_mix.h"
#include "cl_particle.h"
#include "cl_actor.h"
#include "cl_parse.h"
#include "renderer/r_mesh_anim.h"
#include "../common/tracing.h"

localModel_t LMs[MAX_LOCALMODELS];

le_t LEs[MAX_EDICTS];

/*===========================================================================
Local Model (LM) handling
=========================================================================== */

static const char *leInlineModelList[MAX_EDICTS + 1];

static inline void LE_GenerateInlineModelList (void)
{
	le_t *le;
	int i, l;

	l = 0;
	for (i = 0, le = LEs; i < cl.numLEs; i++, le++)
		if (le->inuse && le->model1 && le->inlineModelName[0] == '*')
			leInlineModelList[l++] = le->inlineModelName;
	leInlineModelList[l] = NULL;
}

/**
 * @sa G_CompleteRecalcRouting
 */
void CL_CompleteRecalcRouting (void)
{
	le_t* le;
	int i;

	LE_GenerateInlineModelList();

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++)
		/* We ALWAYS check against a model, even if it isn't in use.
		 * An unused model is NOT included in the inline list, so it doesn't get
		 * traced against. */
		if (le->model1 && le->inlineModelName[0] == '*')
			Grid_RecalcRouting(clMap, le->inlineModelName, leInlineModelList);
}

/**
 * @sa CL_Explode
 * @param[in] le A local entity of a brush model (func_breakable, func_door, ...)
 */
void CL_RecalcRouting (const le_t* le)
{
	LE_GenerateInlineModelList();
	/* We ALWAYS check against a model, even if it isn't in use.
	 * An unused model is NOT included in the inline list, so it doesn't get
	 * traced against. */
	if (le->model1 && le->inlineModelName[0] == '*')
		Grid_RecalcRouting(clMap, le->inlineModelName, leInlineModelList);

	CL_ConditionalMoveCalcActor(selActor);
}

/**
 * @brief Add the local models to the scene
 * @sa V_RenderView
 * @sa LE_AddToScene
 * @sa LM_AddModel
 */
void LM_AddToScene (void)
{
	localModel_t *lm;
	entity_t ent;
	int i;

	for (i = 0, lm = LMs; i < cl.numLMs; i++, lm++) {
		if (!lm->inuse)
			continue;

		/* check for visibility */
		if (!((1 << cl_worldlevel->integer) & lm->levelflags))
			continue;

		/* set entity values */
		memset(&ent, 0, sizeof(ent));
		VectorCopy(lm->origin, ent.origin);
		VectorCopy(lm->origin, ent.oldorigin);
		VectorCopy(lm->angles, ent.angles);
		VectorCopy(lm->scale, ent.scale);
		assert(lm->model);
		ent.model = lm->model;
		ent.skinnum = lm->skin;
		ent.as.frame = lm->frame;
		if (lm->animname[0]) {
			ent.as = lm->as;
			/* do animation */
			R_AnimRun(&lm->as, ent.model, cls.frametime * 1000);
			lm->lighting.dirty = qtrue;
		}

		/* renderflags like RF_PULSE */
		ent.flags = lm->renderFlags;
		ent.lighting = &lm->lighting;

		/* add it to the scene */
		R_AddEntity(&ent);
	}
}

/**
 * @brief Checks whether a local model with the same entity number is already registered
 */
static inline localModel_t *LM_Find (int entnum)
{
	int i;

	for (i = 0; i < cl.numLMs; i++)
		if (LMs[i].entnum == entnum)
			return &LMs[i];

	return NULL;
}

/**
 * @brief Checks whether the given le is a living actor
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 */
qboolean LE_IsActor (const le_t *le)
{
	assert(le);
	return le->type == ET_ACTOR || le->type == ET_ACTOR2x2 || le->type == ET_ACTORHIDDEN;
}

/**
 * @brief Checks whether the given le is a living actor (but might be hidden)
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 * @sa LE_IsActor
 */
qboolean LE_IsLivingActor (const le_t *le)
{
	assert(le);
	return LE_IsActor(le) && !LE_IsDead(le);
}

/**
 * @brief Checks whether the given le is a living and visible actor
 * @param[in] le The local entity to perform the check for
 * @sa G_IsLivingActor
 * @sa LE_IsActor
 */
qboolean LE_IsLivingAndVisibleActor (const le_t *le)
{
	assert(le);
	if (le->invis)
		return qfalse;

	assert(le->type != ET_ACTORHIDDEN);

	return LE_IsLivingActor(le);
}

/**
 * @brief Register misc_models
 * @sa V_LoadMedia
 */
void LM_Register (void)
{
	localModel_t *lm;
	int i;

	for (i = 0, lm = LMs; i < cl.numLMs; i++, lm++) {
		/* register the model */
		lm->model = R_RegisterModelShort(lm->name);
		if (lm->animname[0]) {
			R_AnimChange(&lm->as, lm->model, lm->animname);
			if (!lm->as.change)
				Com_Printf("LM_Register: Could not change anim of model '%s'\n", lm->animname);
		}
		if (!lm->model)
			lm->inuse = qfalse;
	}
}


/**
 * @brief Adds local (not known or handled by the server) models to the map
 * @note misc_model
 * @sa CL_ParseEntitystring
 * @param[in] entnum Entity number
 * @sa LM_AddToScene
 */
localModel_t *LM_AddModel (const char *model, const char *particle, const vec3_t origin, const vec3_t angles, int entnum, int levelflags, int renderFlags, const vec3_t scale)
{
	localModel_t *lm;

	lm = &LMs[cl.numLMs++];

	if (cl.numLMs >= MAX_LOCALMODELS)
		Com_Error(ERR_DROP, "Too many local models\n");

	memset(lm, 0, sizeof(*lm));
	Q_strncpyz(lm->name, model, sizeof(lm->name));
	Q_strncpyz(lm->particle, particle, sizeof(lm->particle));
	VectorCopy(origin, lm->origin);
	VectorCopy(angles, lm->angles);
	/* check whether there is already a model with that number */
	if (LM_Find(entnum))
		Com_Error(ERR_DROP, "Already a local model with the same id (%i) loaded\n", entnum);
	lm->entnum = entnum;
	lm->levelflags = levelflags;
	lm->renderFlags = renderFlags;
	lm->lighting.dirty = qtrue;
	lm->inuse = qtrue;
	VectorCopy(scale, lm->scale);

	return lm;
}

static const float mapZBorder = -(UNIT_HEIGHT * 5);
/**
 * @brief Checks whether give position is still inside the map borders
 */
qboolean CL_OutsideMap (const vec3_t impact, const float delta)
{
	if (impact[0] < mapMin[0] - delta || impact[0] > mapMax[0] + delta)
		return qtrue;

	if (impact[1] < mapMin[1] - delta || impact[1] > mapMax[1] + delta)
		return qtrue;

	/* if a le is deeper than 5 levels below the latest walkable level (0) then
	 * we can assume that it is outside the world
	 * This is needed because some maps (e.g. the dam map) has unwalkable levels
	 * that just exists for detail reasons */
	if (impact[2] < mapZBorder)
		return qtrue;

	/* still inside the map borders */
	return qfalse;
}

/*===========================================================================
LE thinking
=========================================================================== */

/**
 * @brief Calls the le think function
 * @sa LET_StartIdle
 * @sa LET_PathMove
 * @sa LET_StartPathMove
 * @sa LET_Projectile
 */
void LE_Think (void)
{
	le_t *le;
	int i;

	if (cls.state != ca_active)
		return;

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++) {
		if (le->inuse && le->think)
			/* call think function */
			le->think(le);
	}
}


/*===========================================================================
 LE think functions
=========================================================================== */

static char retAnim[MAX_VAR];

/**
 * @brief Get the correct animation for the given actor state and weapons
 * @param[in] right ods index to determine the weapon in the actors right hand
 * @param[in] left ods index to determine the weapon in the actors left hand
 * @param[in] state the actors state - e.g. STATE_CROUCHED (crounched animations)
 * have a 'c' in front of their animation definitions (see *.anm files for
 * characters)
 */
const char *LE_GetAnim (const char *anim, int right, int left, int state)
{
	char *mod;
	qboolean akimbo;
	char animationIndex;
	const char *type;
	size_t length = sizeof(retAnim);

	if (!anim)
		return "";

	mod = retAnim;

	/* add crouched flag */
	if (state & STATE_CROUCHED) {
		*mod++ = 'c';
		length--;
	}

	/* determine relevant data */
	akimbo = qfalse;
	if (right == NONE) {
		animationIndex = '0';
		if (left == NONE)
			type = "item";
		else {
			/* left hand grenades look OK with default anim; others don't */
			if (strcmp(csi.ods[left].type, "grenade"))
				akimbo = qtrue;
			type = csi.ods[left].type;
		}
	} else {
		animationIndex = csi.ods[right].animationIndex;
		type = csi.ods[right].type;
		if (left != NONE && !strcmp(csi.ods[right].type, "pistol") && !strcmp(csi.ods[left].type, "pistol"))
			akimbo = qtrue;
	}

	if (!strncmp(anim, "stand", 5) || !strncmp(anim, "walk", 4)) {
		Q_strncpyz(mod, anim, length);
		mod += strlen(anim);
		*mod++ = animationIndex;
		*mod++ = 0;
	} else {
		Com_sprintf(mod, length, "%s_%s", anim, akimbo ? "pistol_d" : type);
	}

	return retAnim;
}

/**
 * @brief Change the animation of an actor to the idle animation (which can be
 * panic, dead or stand)
 * @note We have more than one animation for dead - the index is given by the
 * state of the local entity
 * @note Think function
 * @note See the *.anm files in the models dir
 */
void LET_StartIdle (le_t * le)
{
	/* hidden actors don't have models assigned, thus we can not change the
	 * animation for any model */
	if (le->type != ET_ACTORHIDDEN) {
		if (LE_IsDead(le))
			R_AnimChange(&le->as, le->model1, va("dead%i", LE_GetAnimationIndexForDeath(le)));
		else if (le->state & STATE_PANIC)
			R_AnimChange(&le->as, le->model1, "panic0");
		else
			R_AnimChange(&le->as, le->model1, LE_GetAnim("stand", le->right, le->left, le->state));
	}

	le->pathPos = le->pathLength = 0;

	/* keep this animation until something happens */
	le->think = NULL;
}

/**
 * @param[in] contents The contents flash of the brush we are currently in
 */
static void LE_PlaySoundFileForContents (le_t* le, int contents)
{
	/* only play those water sounds when an actor jumps into the water - but not
	 * if he enters carefully in crouched mode */
	if (le->state & ~STATE_CROUCHED) {
		if (contents & CONTENTS_WATER) {
			/* were we already in the water? */
			if (le->positionContents & CONTENTS_WATER) {
				/* play water moving sound */
				S_PlaySample(le->origin, cls.soundPool[SOUND_WATER_OUT], SOUND_ATTN_IDLE, SND_VOLUME_FOOTSTEPS);
			} else {
				/* play water entering sound */
				S_PlaySample(le->origin, cls.soundPool[SOUND_WATER_IN], SOUND_ATTN_IDLE, SND_VOLUME_FOOTSTEPS);
			}
			return;
		}

		if (le->positionContents & CONTENTS_WATER) {
			/* play water leaving sound */
			S_PlaySample(le->origin, cls.soundPool[SOUND_WATER_MOVE], SOUND_ATTN_IDLE, SND_VOLUME_FOOTSTEPS);
		}
	}
}

/**
 * @brief Plays step sounds and draw particles for different terrain types
 * @param[in] le The local entity to play the sound and draw the particle for
 * @param[in] textureName The name of the texture the actor is standing on
 * @sa LET_PathMove
 */
static void LE_PlaySoundFileAndParticleForSurface (le_t* le, const char *textureName)
{
	const terrainType_t *t;
	vec3_t origin;

	t = Com_GetTerrainType(textureName);
	if (!t)
		return;

	/* origin might not be up-to-date here - but pos should be */
	PosToVec(le->pos, origin);

	/** @todo use the Grid_Fall method (ACTOR_SIZE_NORMAL) to ensure, that the particle is
	 * drawn at the ground (if needed - maybe the origin is already ground aligned)*/
	if (t->particle) {
		/* check whether actor is visible */
		if (LE_IsLivingAndVisibleActor(le))
			CL_ParticleSpawn(t->particle, 0, origin, NULL, NULL);
	}
	if (t->footStepSound) {
		s_sample_t *sample = S_LoadSample(t->footStepSound);
		Com_DPrintf(DEBUG_SOUND, "LE_PlaySoundFileAndParticleForSurface: volume %.2f\n", t->footStepVolume);
		S_PlaySample(origin, sample, SOUND_ATTN_STATIC, t->footStepVolume);
	}
}

/**
 * @brief Searches the closest actor to the given world vector
 * @param[in] origin World position to get the closest actor to
 * @note Only your own team is searched
 */
le_t* LE_GetClosestActor (const vec3_t origin)
{
	int i, tmp;
	int dist = 999999;
	le_t *actor = NULL, *le;
	vec3_t leOrigin;

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++) {
		if (!le->inuse || le->pnum != cl.pnum)
			continue;
		/* visible because it's our team - so we just check for living actor here */
		if (!LE_IsLivingActor(le))
			continue;
		VectorSubtract(origin, le->origin, leOrigin);
		tmp = VectorLength(leOrigin);
		if (tmp < dist) {
			actor = le;
			dist = tmp;
		}
	}

	return actor;
}

/**
 * @brief Move the actor along the path to the given location
 * @note Think function
 * @sa CL_ActorDoMove
 */
static void LET_PathMove (le_t * le)
{
	float frac;
	vec3_t start, dest, delta;

	/* check for start */
	if (cl.time <= le->startTime)
		return;

	/* move ahead */
	while (cl.time > le->endTime) {
		/* Ensure that we are displayed where we are supposed to be, in case the last frame came too quickly. */
		Grid_PosToVec(clMap, le->fieldSize, le->pos, le->origin);

		/* Record the last position of movement calculations. */
		VectorCopy(le->pos, le->oldPos);

		if (le->pathPos < le->pathLength) {
			/* next part */
			const byte fulldv = le->path[le->pathPos];
			const byte dir = getDVdir(fulldv);
			const byte crouchingState = le->state & STATE_CROUCHED ? 1 : 0;
			/** @note newCrouchingState needs to be set to the current crouching state and is possibly updated by PosAddDV. */
			byte newCrouchingState = crouchingState;
			PosAddDV(le->pos, newCrouchingState, fulldv);

			/* walking in water will not play the normal footstep sounds */
			if (!le->pathContents[le->pathPos]) {
				trace_t trace;
				vec3_t from, to;

				/* prepare trace vectors */
				PosToVec(le->pos, from);
				VectorCopy(from, to);
				/* we should really hit the ground with this */
				to[2] -= UNIT_HEIGHT;

				trace = CL_Trace(from, to, vec3_origin, vec3_origin, NULL, NULL, MASK_SOLID);
				if (trace.surface)
					LE_PlaySoundFileAndParticleForSurface(le, trace.surface->name);
			} else
				LE_PlaySoundFileForContents(le, le->pathContents[le->pathPos]);

			/* only change the direction if the actor moves horizontally. */
			if (dir < CORE_DIRECTIONS || dir >= FLYING_DIRECTIONS)
				le->dir = dir & (CORE_DIRECTIONS - 1);
			le->angles[YAW] = directionAngles[le->dir];
			le->startTime = le->endTime;
			/* check for straight movement or diagonal movement */
			assert(le->speed[le->pathPos]);
			if (dir != DIRECTION_FALL) {
				/* sqrt(2) for diagonal movement */
				le->endTime += (le->dir >= BASE_DIRECTIONS ? UNIT_SIZE * 1.41 : UNIT_SIZE) * 1000 / le->speed[le->pathPos];
			} else {
				/* This needs to account for the distance of the fall. */
				Grid_PosToVec(clMap, le->fieldSize, le->oldPos, start);
				Grid_PosToVec(clMap, le->fieldSize, le->pos, dest);
				/* 1/1000th of a second per model unit in height change */
				le->endTime += (start[2] - dest[2]);
			}

			le->positionContents = le->pathContents[le->pathPos];
			le->pathPos++;
		} else {
			/* end of move */
			le_t *floor;

			/* Verify the current position */
			if (!VectorCompare(le->pos, le->newPos))
				Com_Error(ERR_DROP, "LET_PathMove: Actor movement is out of sync: %i:%i:%i should be %i:%i:%i (step %i of %i) (team %i)",
						le->pos[0], le->pos[1], le->pos[2], le->newPos[0], le->newPos[1], le->newPos[2], le->pathPos, le->pathLength, le->team);

			CL_ConditionalMoveCalcActor(le);

			/* link any floor container into the actor temp floor container */
			floor = LE_Find(ET_ITEM, le->pos);
			if (floor)
				FLOOR(le) = FLOOR(floor);

			le->lighting.dirty = qtrue;
			le->think = LET_StartIdle;
			return;
		}
	}

	/* interpolate the position */
	Grid_PosToVec(clMap, le->fieldSize, le->oldPos, start);
	Grid_PosToVec(clMap, le->fieldSize, le->pos, dest);
	VectorSubtract(dest, start, delta);

	frac = (float) (cl.time - le->startTime) / (float) (le->endTime - le->startTime);

	le->lighting.dirty = qtrue;

	/* calculate the new interpolated actor origin in the world */
	VectorMA(start, frac, delta, le->origin);
}

/**
 * @note Think function
 * @brief Change the actors animation to walking
 * @note See the *.anm files in the models dir
 */
void LET_StartPathMove (le_t *le)
{
	/* initial animation or animation change */
	R_AnimChange(&le->as, le->model1, LE_GetAnim("walk", le->right, le->left, le->state));
	if (!le->as.change)
		Com_Printf("LET_StartPathMove: Could not change anim of le: %i, team: %i, pnum: %i\n",
			le->entnum, le->team, le->pnum);

	le->think = LET_PathMove;
	le->think(le);
}

/**
 * @note Think function
 */
static void LET_Projectile (le_t * le)
{
	if (cl.time >= le->endTime) {
		vec3_t impact;
		VectorCopy(le->origin, impact);
		CL_ParticleFree(le->ptl);
		/* don't run the think function again */
		le->inuse = qfalse;
		if (le->ref1 && le->ref1[0]) {
			VectorCopy(le->ptl->s, impact);
			/** @todo Why are we using le->state here, not le->dir? */
			le->ptl = CL_ParticleSpawn(le->ref1, 0, impact, bytedirs[le->state], NULL);
			VecToAngles(bytedirs[le->state], le->ptl->angles);
		}
		if (le->ref2 && le->ref2[0]) {
			s_sample_t *sample = S_LoadSample(le->ref2);
			S_PlaySample(impact, sample, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
		}
	} else if (CL_OutsideMap(le->ptl->s, UNIT_SIZE * 10)) {
		le->endTime = cl.time;
		CL_ParticleFree(le->ptl);
		/* don't run the think function again */
		le->inuse = qfalse;
	}
}

/*===========================================================================
 LE Special Effects
=========================================================================== */

void LE_AddProjectile (const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t impact, int normal)
{
	le_t *le;
	vec3_t delta;
	float dist;

	/* add le */
	le = LE_Add(0);
	if (!le)
		return;
	le->invis = !cl_leshowinvis->integer;
	/* bind particle */
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle, NULL, NULL);
	if (!le->ptl) {
		le->inuse = qfalse;
		return;
	}

	/* calculate parameters */
	VectorSubtract(impact, muzzle, delta);
	dist = VectorLength(delta);

	VecToAngles(delta, le->ptl->angles);
	/* direction */
	/** @todo Why are we not using le->dir here?
	 * @sa LE_AddGrenade */
	le->state = normal;
	le->fd = fd;

	/* infinite speed projectile? */
	if (!fd->speed) {
		le->inuse = qfalse;
		le->ptl->size[0] = dist;
		VectorMA(muzzle, 0.5, delta, le->ptl->s);
		if (flags & (SF_IMPACT | SF_BODY) || (fd->splrad && !fd->bounce)) {
			ptl_t *ptl = NULL;
			if (flags & SF_BODY) {
				if (fd->hitBodySound[0]) {
					s_sample_t *sample = S_LoadSample(fd->hitBodySound);
					S_PlaySample(le->origin, sample, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
				}
				if (fd->hitBody[0])
					ptl = CL_ParticleSpawn(fd->hitBody, 0, impact, bytedirs[normal], NULL);
			} else {
				if (fd->impactSound[0]) {
					s_sample_t *sample = S_LoadSample(fd->impactSound);
					S_PlaySample(le->origin, sample, le->fd->impactAttenuation, SND_VOLUME_WEAPONS);
				}
				if (fd->impact[0])
					ptl = CL_ParticleSpawn(fd->impact, 0, impact, bytedirs[normal], NULL);
			}
			if (ptl)
				VecToAngles(bytedirs[normal], ptl->angles);
		}
		return;
	}
	/* particle properties */
	VectorScale(delta, fd->speed / dist, le->ptl->v);
	le->endTime = cl.time + 1000 * dist / fd->speed;

	/* think function */
	if (flags & SF_BODY) {
		le->ref1 = fd->hitBody;
		le->ref2 = fd->hitBodySound;
	} else if (flags & SF_IMPACT || (fd->splrad && !fd->bounce)) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	le->think = LET_Projectile;
	le->think(le);
}

/**
 * @brief Returns the index of the biggest item in the inventory list
 * @note This item is the only one that will be drawn when lying at the floor
 * @sa LE_PlaceItem
 * @return the item index in the @c csi.ods array
 * @note Only call this for none empty invList_t - see FLOOR, LEFT, RIGHT and so on macros
 */
static objDef_t *LE_BiggestItem (const invList_t *ic)
{
	objDef_t *max;
	int maxSize = 0;

	for (max = ic->item.t; ic; ic = ic->next) {
		const int size = Com_ShapeUsage(ic->item.t->shape);
		if (size > maxSize) {
			max = ic->item.t;
			maxSize = size;
		}
	}

	/* there must be an item in the invList_t */
	assert(max);
	return max;
}

/**
 * @sa CL_BiggestItem
 * @param[in] le The local entity (ET_ITEM) with the floor container
 */
void LE_PlaceItem (le_t *le)
{
	le_t *actor;
	int i;

	assert(le->type == ET_ITEM);

	/* search owners (there can be many, some of them dead) */
	for (i = 0, actor = LEs; i < cl.numLEs; i++, actor++)
		if (actor->inuse && (actor->type == ET_ACTOR || actor->type == ET_ACTOR2x2)
		 && VectorCompare(actor->pos, le->pos)) {
			if (FLOOR(le))
				FLOOR(actor) = FLOOR(le);
		}

	/* the le is an ET_ITEM entity, this entity is there to render dropped items
	 * if there are no items in the floor container, this entity can be
	 * deactivated */
	if (FLOOR(le)) {
		const objDef_t *biggest = LE_BiggestItem(FLOOR(le));
		le->model1 = cls.modelPool[biggest->idx];
		if (!le->model1)
			Com_Error(ERR_DROP, "Model for item %s is not precached in the cls.model_weapons array",
				biggest->id);
		Grid_PosToVec(clMap, le->fieldSize, le->pos, le->origin);
		VectorSubtract(le->origin, biggest->center, le->origin);
		le->angles[ROLL] = 90;
		/*le->angles[YAW] = 10*(int)(le->origin[0] + le->origin[1] + le->origin[2]) % 360; */
		le->origin[2] -= GROUND_DELTA;
	} else {
		/* If no items in floor inventory, don't draw this le - the container is
		 * maybe empty because an actor picked up the last items here */
		/** @todo This will prevent LE_Add to get a floor-edict when in
		 * mid-move. @sa g_client.c:G_ClientInvMove.
		 * (It will cause asserts/segfaults in e.g. MN_DrawContainerNode)
		 * mattn: But why do we want to get an empty container? If we don't set
		 * this to qfalse we get the null model rendered, because the le->model
		 * is @c NULL, too.
		 * ---
		 * I disabled the line again, because I really like having no segfauls/asserts.
		 * There has to be a better solution/fix for the lila/null dummy models. --Hoehrer 2008-08-27
		 * See the following link for details:
		 * https://sourceforge.net/tracker/index.php?func=detail&aid=2071463&group_id=157793&atid=805242
		le->inuse = qfalse;
		*/
		Com_Error(ERR_DROP, "Empty container as floor le with number: %i", le->entnum);
	}
}

/**
 * @param[in] fd The grenade fire definition
 * @param[in] flags bitmask: SF_BODY, SF_IMPACT, SF_BOUNCING, SF_BOUNCED
 * @param[in] muzzle starting/location vector
 * @param[in] v0 velocity vector
 * @param[in] dt delta seconds
 */
void LE_AddGrenade (const fireDef_t *fd, int flags, const vec3_t muzzle, const vec3_t v0, int dt)
{
	le_t *le;
	vec3_t accel;

	/* add le */
	le = LE_Add(0);
	if (!le)
		return;
	le->invis = !cl_leshowinvis->integer;

	/* bind particle */
	VectorSet(accel, 0, 0, -GRAVITY);
	le->ptl = CL_ParticleSpawn(fd->projectile, 0, muzzle, v0, accel);
	if (!le->ptl) {
		le->inuse = qfalse;
		return;
	}
	/* particle properties */
	VectorSet(le->ptl->angles, 360 * crand(), 360 * crand(), 360 * crand());
	VectorSet(le->ptl->omega, 500 * crand(), 500 * crand(), 500 * crand());

	/* think function */
	if (flags & SF_BODY) {
		le->ref1 = fd->hitBody;
		le->ref2 = fd->hitBodySound;
	} else if (flags & SF_IMPACT || (fd->splrad && !fd-> bounce)) {
		le->ref1 = fd->impact;
		le->ref2 = fd->impactSound;
	} else {
		le->ref1 = NULL;
		if (flags & SF_BOUNCING)
			le->ref2 = fd->bounceSound;
	}

	le->endTime = cl.time + dt;
	/** @todo Why are we not using le->dir here?
	 @sa LE_AddProjectile */
	le->state = 5;				/* direction (0,0,1) */
	le->fd = fd;
	assert(fd);
	le->think = LET_Projectile;
	le->think(le);
}

/**
 * @brief Add function for brush models
 * @sa LE_AddToScene
 */
qboolean LE_BrushModelAction (le_t * le, entity_t * ent)
{
	switch (le->type) {
	case ET_ROTATING:
	case ET_DOOR:
		/* These cause the model to render correctly */
		VectorCopy(ent->mins, le->mins);
		VectorCopy(ent->maxs, le->maxs);
		VectorCopy(ent->origin, le->origin);
		VectorCopy(ent->angles, le->angles);
		break;
	case ET_BREAKABLE:
		break;
	}

	return qtrue;
}

void LET_BrushModel (le_t *le)
{
	/** @todo what is le->speed for a brush model? */
	if (cl.time - le->thinkDelay < le->speed[0]) {
		le->thinkDelay = cl.time;
		return;
	}

	if (le->type == ET_ROTATING) {
		const float angle = le->angles[le->dir] + (1.0 / le->rotationSpeed);
		le->angles[le->dir] = (angle >= 360.0 ? angle - 360.0 : angle);
	}
}

/**
 * @brief Adds ambient sounds from misc_sound entities
 * @sa CL_ParseEntitystring
 */
void LE_AddAmbientSound (const char *sound, const vec3_t origin, int levelflags, float volume)
{
	le_t* le;
	s_sample_t* sample;

	if (strstr(sound, "sound/"))
		sound += 6;

	sample = S_LoadSample(sound);
	if (!sample)
		return;

	le = LE_Add(0);
	if (!le) {
		Com_Printf("Could not add ambient sound entity\n");
		return;
	}
	le->type = ET_SOUND;
	le->sample = sample;
	VectorCopy(origin, le->origin);
	le->invis = !cl_leshowinvis->integer;
	le->levelflags = levelflags;

	if (volume < 0.0f || volume > 1.0f) {
		le->volume = SND_VOLUME_DEFAULT;
		Com_Printf("Invalid volume for local entity given - only values between 0.0 and 1.0 are valid\n");
	} else {
		le->volume = volume;
	}

	Com_DPrintf(DEBUG_SOUND, "Add ambient sound '%s' with volume %f\n", sound, volume);
}

/*===========================================================================
 LE Management functions
=========================================================================== */

/**
 * @brief Add a new local entity to the scene
 * @param[in] entnum The entity number (server side)
 * @sa LE_Get
 */
le_t *LE_Add (int entnum)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++)
		if (!le->inuse)
			/* found a free LE */
			break;

	/* list full, try to make list longer */
	if (i == cl.numLEs) {
		if (cl.numLEs >= MAX_EDICTS) {
			/* no free LEs */
			Com_Error(ERR_DROP, "Too many LEs");
		}

		/* list isn't too long */
		cl.numLEs++;
	}

	/* initialize the new LE */
	memset(le, 0, sizeof(*le));
	le->inuse = qtrue;
	le->entnum = entnum;
	le->fieldSize = ACTOR_SIZE_NORMAL;
	return le;
}

void _LE_NotFoundError (const int entnum, const char *file, const int line)
{
	Cmd_ExecuteString("debug_listle");
	Com_Error(ERR_DROP, "LE_NotFoundError: Could not get LE with entnum %i (%s:%i)\n", entnum, file, line);
}

/**
 * @brief Searches all local entities for the one with the searched entnum
 * @param[in] entnum The entity number (server side)
 * @sa LE_Add
 */
le_t *LE_Get (int entnum)
{
	int i;
	le_t *le;

	if (entnum == SKIP_LOCAL_ENTITY)
		return NULL;

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++)
		if (le->inuse && le->entnum == entnum)
			/* found the LE */
			return le;

	/* didn't find it */
	return NULL;
}

/**
 * @brief Searches a local entity on a given grid field
 * @param[in] type Entity type
 * @param[in] pos The grid pos to search for an item of the given type
 */
le_t *LE_Find (int type, pos3_t pos)
{
	int i;
	le_t *le;

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++)
		if (le->inuse && le->type == type && VectorCompare(le->pos, pos))
			/* found the LE */
			return le;

	/* didn't find it */
	return NULL;
}

/** @sa BoxOffset in cl_actor.c */
#define ModelOffset(i, target) (target[0]=(i-1)*(UNIT_SIZE+BOX_DELTA_WIDTH)/2, target[1]=(i-1)*(UNIT_SIZE+BOX_DELTA_LENGTH)/2, target[2]=0)

/**
 * @sa V_RenderView
 * @sa CL_AddUGV
 * @sa CL_AddActor
 */
void LE_AddToScene (void)
{
	le_t *le;
	entity_t ent;
	vec3_t modelOffset;
	int i;

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++) {
		if (le->inuse && !le->invis) {
			if (le->contents & CONTENTS_SOLID) {
				if (!((1 << cl_worldlevel->integer) & le->levelflags))
					continue;
			} else if (le->contents & CONTENTS_DETAIL) {
				/* show them always */
			} else if (le->pos[2] > cl_worldlevel->integer)
				continue;

			memset(&ent, 0, sizeof(ent));

			ent.alpha = le->alpha;
			ent.state = le->state;

			VectorCopy(le->angles, ent.angles);
			ent.model = le->model1;
			ent.skinnum = le->skinnum;

			switch (le->contents) {
			/* Only breakables do not use their origin; func_doors and func_rotating do!!!
			 * But none of them have animations. */
			case CONTENTS_SOLID:
			case CONTENTS_DETAIL: /* they use mins/maxs */
				break;
			default:
				/* set entity values */
				VectorCopy(le->origin, ent.origin);
				VectorCopy(le->origin, ent.oldorigin);

				/* do animation */
				R_AnimRun(&le->as, ent.model, cls.frametime * 1000);
				ent.as = le->as;
				break;
			}

			if (le->type == ET_DOOR || le->type == ET_ROTATING) {
				VectorCopy(le->angles, ent.angles);
				VectorCopy(le->origin, ent.origin);
				VectorCopy(le->origin, ent.oldorigin);
			}

			/**
			 * Offset the model to be inside the cursor box
			 * @todo Dunno if this is the best place to do it - what happens to
			 * shot-origin and stuff? le->origin is never changed.
			 */
			switch (le->fieldSize) {
			case ACTOR_SIZE_NORMAL:
			case ACTOR_SIZE_2x2:
				ModelOffset(le->fieldSize, modelOffset);
				VectorAdd(ent.origin, modelOffset, ent.origin);
				VectorAdd(ent.oldorigin, modelOffset, ent.oldorigin);
				break;
			default:
				break;
			}

			ent.lighting = &le->lighting;

			/* call add function */
			/* if it returns false, don't draw */
			if (le->addFunc)
				if (!le->addFunc(le, &ent))
					continue;

			/* add it to the scene */
			R_AddEntity(&ent);
		}
	}
}

/**
 * @brief Cleanup unused LE inventories that the server sent to the client
 * also free some unused LE memory
 */
void LE_Cleanup (void)
{
	int i;
	le_t *le;
	inventory_t inv;

	Com_DPrintf(DEBUG_CLIENT, "LE_Cleanup: Clearing up to %i unused LE inventories\n", cl.numLEs);
	for (i = cl.numLEs - 1, le = &LEs[cl.numLEs - 1]; i >= 0; i--, le--) {
		if (!le->inuse)
			continue;
		switch (le->type) {
		case ET_ACTOR:
		case ET_ACTOR2x2:
			inv = le->i;
			CL_ActorCleanup(le);
			break;
		case ET_ITEM:
			INVSH_EmptyContainer(&le->i, &csi.ids[csi.idFloor]);
			break;
		}
	}
}

#ifdef DEBUG
/**
 * @brief Shows a list of current know local entities with type and status
 */
void LE_List_f (void)
{
	int i;
	le_t *le;

	Com_Printf("number | entnum | type | inuse | invis | pnum | team | size |  HP | state | level | model/ptl\n");
	for (i = 0, le = LEs; i < cl.numLEs; i++, le++) {
		Com_Printf("#%5i | #%5i | %4i | %5i | %5i | %4i | %4i | %4i | %3i | %5i | %5i | ",
			i, le->entnum, le->type, le->inuse, le->invis, le->pnum, le->team,
			le->fieldSize, le->HP, le->state, le->levelflags);
		if (le->type == ET_PARTICLE) {
			if (le->ptl)
				Com_Printf("%s\n", le->ptl->ctrl->name);
			else
				Com_Printf("no ptl\n");
		} else if (le->model1)
			Com_Printf("%s\n", le->model1->name);
		else
			Com_Printf("no mdl\n");
	}
}

/**
 * @brief Shows a list of current know local models
 */
void LM_List_f (void)
{
	int i;
	localModel_t *lm;

	Com_Printf("number | entnum | skin | frame | lvlflg | renderflags | origin          | name\n");
	for (i = 0, lm = LMs; i < cl.numLMs; i++, lm++) {
		Com_Printf("#%5i | #%5i | #%3i | #%4i | %6i | %11i | %5.0f:%5.0f:%3.0f | %s\n",
			i, lm->entnum, lm->skin, lm->frame, lm->levelflags, lm->renderFlags,
			lm->origin[0], lm->origin[1], lm->origin[2], lm->name);
	}
}

#endif

/*===========================================================================
 LE Tracing
=========================================================================== */

/** @brief Client side moveclip */
typedef struct {
	vec3_t boxmins, boxmaxs;	/**< enclose the test object along entire move */
	const float *mins, *maxs;	/**< size of the moving object */
	float *start, *end;
	trace_t trace;
	le_t *passle, *passle2;		/**< ignore these for clipping */
	int contentmask;			/**< search these in your trace - see MASK_* */
} moveclip_t;


/**
 * @brief Clip against solid entities
 * @sa CL_Trace
 * @sa SV_ClipMoveToEntities
 */
static void CL_ClipMoveToLEs (moveclip_t * clip)
{
	int i, tile = 0;
	le_t *le;
	trace_t trace;
	int headnode;
	cBspModel_t *model;
	const float *angles;
	vec3_t origin;

	if (clip->trace.allsolid)
		return;

	for (i = 0, le = LEs; i < cl.numLEs; i++, le++) {
		if (!le->inuse || !(le->contents & clip->contentmask))
			continue;
		if (le == clip->passle || le == clip->passle2)
			continue;

		/* special case for bmodels */
		if (le->contents & CONTENTS_SOLID) {
			/* special value for bmodel */
			assert(le->modelnum1 < MAX_MODELS);
			model = cl.model_clip[le->modelnum1];
			if (!model) {
				Com_Printf("CL_ClipMoveToLEs: Error - le with no NULL bmodel (%i)\n", le->type);
				continue;
			}
			headnode = model->headnode;
			tile = model->tile;
			angles = le->angles;
		} else {
			/* might intersect, so do an exact clip */
			/** @todo make headnode = HullForLe(le, &tile) ... the counterpart of SV_HullForEntity in server/sv_world.c */
			headnode = CM_HeadnodeForBox(0, le->mins, le->maxs);
			angles = vec3_origin;
		}
		VectorCopy(le->origin, origin);

		assert(headnode < MAX_MAP_NODES);
		trace = CM_TransformedBoxTrace(tile, clip->start, clip->end, clip->mins, clip->maxs, headnode, clip->contentmask, 0, origin, angles);

		if (trace.fraction < clip->trace.fraction) {
			qboolean oldStart;

			/* make sure we keep a startsolid from a previous trace */
			oldStart = clip->trace.startsolid;
			trace.le = le;
			clip->trace = trace;
			clip->trace.startsolid |= oldStart;
		/* if true, plane is not valid */
		} else if (trace.allsolid) {
			trace.le = le;
			clip->trace = trace;
		/* if true, the initial point was in a solid area */
		} else if (trace.startsolid) {
			trace.le = le;
			clip->trace.startsolid = qtrue;
		}
	}
}


/**
 * @brief Create the bounding box for the entire move
 * @param[in] start Start vector to start the trace from
 * @param[in] mins Bounding box used for tracing
 * @param[in] maxs Bounding box used for tracing
 * @param[in] end End vector to stop the trace at
 * @param[out] boxmins The resulting box mins
 * @param[out] boxmaxs The resulting box maxs
 * @sa CL_Trace
 * @note Box is expanded by 1
 */
static inline void CL_TraceBounds (const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, vec3_t boxmins, vec3_t boxmaxs)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (end[i] > start[i]) {
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		} else {
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

/**
 * @brief Moves the given mins/maxs volume through the world from start to end.
 * @note Passedict and edicts owned by passedict are explicitly not checked.
 * @sa CL_TraceBounds
 * @sa CL_ClipMoveToLEs
 * @sa SV_Trace
 * @param[in] start Start vector to start the trace from
 * @param[in] end End vector to stop the trace at
 * @param[in] mins Bounding box used for tracing
 * @param[in] maxs Bounding box used for tracing
 * @param[in] passle Ignore this local entity in the trace (might be NULL)
 * @param[in] passle2 Ignore this local entity in the trace (might be NULL)
 * @param[in] contentmask Searched content the trace should watch for
 * @todo cl_worldlevel->integer should be a function parameter to eliminate sideeffects like e.g. in the particles code
 */
trace_t CL_Trace (vec3_t start, vec3_t end, const vec3_t mins, const vec3_t maxs, le_t * passle, le_t * passle2, int contentmask)
{
	moveclip_t clip;

	/* clip to world */
	clip.trace = TR_CompleteBoxTrace(start, end, mins, maxs, (1 << (cl_worldlevel->integer + 1)) - 1, contentmask, 0);
	clip.trace.le = NULL;
	if (clip.trace.fraction == 0)
		return clip.trace;		/* blocked by the world */

	clip.contentmask = contentmask;
	clip.start = start;
	clip.end = end;
	clip.mins = mins;
	clip.maxs = maxs;
	clip.passle = passle;
	clip.passle2 = passle2;

	/* create the bounding box of the entire move */
	CL_TraceBounds(start, mins, maxs, end, clip.boxmins, clip.boxmaxs);

	/* clip to other solid entities */
	CL_ClipMoveToLEs(&clip);

	return clip.trace;
}
