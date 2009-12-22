/**
 * @file g_match.c
 * @brief Match related functions
 */

/*
All original material Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "g_local.h"
#include "g_ai.h"

/**
 * @brief Determines the amount of XP earned by a given soldier for a given skill, based on the soldier's performance in the last mission.
 * @param[in] skill The skill for which to fetch the maximum amount of XP.
 * @param[in] chr Pointer to the character you want to get the earned experience for
 * @sa G_UpdateCharacterSkills
 * @sa G_GetMaxExperiencePerMission
 */
static int G_GetEarnedExperience (abilityskills_t skill, character_t *chr)
{
	int exp = 0;
	abilityskills_t i;

	switch (skill) {
	case ABILITY_POWER:
		exp = 46; /** @todo Make a formula for this once strength is used in combat. */
		break;
	case ABILITY_SPEED:
		exp = chr->scoreMission->movedNormal / 2 + chr->scoreMission->movedCrouched + (chr->scoreMission->firedTUs[skill] + chr->scoreMission->firedSplashTUs[skill]) / 10;
		break;
	case ABILITY_ACCURACY:
		for (i = 0; i < SKILL_NUM_TYPES; i++) {
			if (i == SKILL_SNIPER)
				exp = 30 * (chr->scoreMission->hits[i][KILLED_ALIENS] + chr->scoreMission->hitsSplash[i][KILLED_ALIENS]);
			else
				exp = 20 * (chr->scoreMission->hits[i][KILLED_ALIENS] + chr->scoreMission->hitsSplash[i][KILLED_ALIENS]);
		}
		break;
	case ABILITY_MIND:
		exp = 100 * chr->scoreMission->kills[KILLED_ALIENS];
		break;
	case SKILL_CLOSE:
		exp = 150 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_HEAVY:
		exp = 200 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_ASSAULT:
		exp = 100 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_SNIPER:
		exp = 200 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	case SKILL_EXPLOSIVE:
		exp = 200 * (chr->scoreMission->hits[skill][KILLED_ALIENS] + chr->scoreMission->hitsSplash[skill][KILLED_ALIENS]);
		break;
	default:
		break;
	}

	return exp;
}

/**
 * @brief Updates character skills after a mission.
 * @param[in] chr Pointer to a character_t.
 * @sa CL_UpdateCharacterStats
 * @sa G_UpdateCharacterScore
 * @sa G_UpdateHitScore
 * @todo Update skill calculation with implant data once implants are implemented
 */
static void G_UpdateCharacterSkills (character_t *chr)
{
	abilityskills_t i = 0;
	unsigned int maxXP, gainedXP, totalGainedXP;

	/* Robots/UGVs do not get skill-upgrades. */
	if (chr->teamDef->race == RACE_ROBOT)
		return;

	totalGainedXP = 0;
	for (i = 0; i < SKILL_NUM_TYPES; i++) {
		maxXP = CHRSH_CharGetMaxExperiencePerMission(i);
		gainedXP = G_GetEarnedExperience(i, chr);

		gainedXP = min(gainedXP, maxXP);
		chr->score.experience[i] += gainedXP;
		totalGainedXP += gainedXP;
		chr->score.skills[i] = chr->score.initialSkills[i] + (int) (pow((float) (chr->score.experience[i])/100, 0.6f));
		G_PrintStats(va("Soldier %s earned %d experience points in skill #%d (total experience: %d). It is now %d higher.\n",
				chr->name, gainedXP, i, chr->score.experience[i], chr->score.skills[i] - chr->score.initialSkills[i]));
	}

	/* Health isn't part of abilityskills_t, so it needs to be handled separately. */
	assert(i == SKILL_NUM_TYPES);	/**< We need to get sure that index for health-experience is correct. */
	maxXP = CHRSH_CharGetMaxExperiencePerMission(i);
	gainedXP = min(maxXP, totalGainedXP / 2);

	chr->score.experience[i] += gainedXP;
	chr->maxHP = chr->score.initialSkills[i] + (int) (pow((float) (chr->score.experience[i]) / 100, 0.6f));
	G_PrintStats(va("Soldier %s earned %d experience points in skill #%d (total experience: %d). It is now %d higher.\n",
			chr->name, gainedXP, i, chr->score.experience[i], chr->maxHP - chr->score.initialSkills[i]));
}

/**
 * Triggers the end of the game. Will be executed in the next server (or game) frame.
 * @param team The winning team
 * @param timeGap Second to wait before really ending the game. Useful if you want to allow a last view on the scene
 */
void G_MatchEndTrigger (int team, int timeGap)
{
	const int realTimeGap = timeGap > 0 ? level.time + timeGap : 1;
	level.winningTeam = team;
	level.intermissionTime = realTimeGap;

	gi.BroadcastPrintf(PRINT_HUD, _("Mission won for team %i\n"), level.winningTeam);
}

/**
 * @brief Sends character stats like assigned missions and kills back to client
 * @note first short is the ucn to allow the client to identify the character
 * @note parsed in GAME_CP_Results
 * @sa GAME_SendCurrentTeamSpawningInfo
 * @note you also have to update the pascal string size in G_MatchSendResults if you change the buffer here
 */
static void G_SendCharacterData (const edict_t* ent)
{
	int k;

	assert(ent);

	/* write character number */
	gi.WriteShort(ent->chr.ucn);

	gi.WriteShort(ent->HP);
	gi.WriteByte(ent->STUN);
	gi.WriteByte(ent->morale);

	/** Scores @sa inv_shared.h:chrScoreGlobal_t */
	for (k = 0; k < SKILL_NUM_TYPES + 1; k++)
		gi.WriteLong(ent->chr.score.experience[k]);
	for (k = 0; k < SKILL_NUM_TYPES; k++)
		gi.WriteByte(ent->chr.score.skills[k]);
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.WriteShort(ent->chr.score.kills[k]);
	for (k = 0; k < KILLED_NUM_TYPES; k++)
		gi.WriteShort(ent->chr.score.stuns[k]);
	gi.WriteShort(ent->chr.score.assignedMissions);
}

/**
 * @brief Handles the end of a match
 * @sa G_RunFrame
 * @sa CL_ParseResults
 */
static void G_MatchSendResults (int team)
{
	edict_t *ent, *attacker;
	int i, j = 0;

	attacker = NULL;
	/* Calculate new scores/skills for the soldiers. */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent)) {
			if (!G_IsAI(ent))
				G_UpdateCharacterSkills(&ent->chr);
			else if (ent->team == team)
				attacker = ent;
		}

	/* if aliens won, make sure every soldier dies */
	if (team == TEAM_ALIEN) {
		for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
			if (ent->inuse && G_IsLivingActor(ent) && ent->team != team)
				G_ActorDie(ent, STATE_DEAD, attacker);
	}

	/* Make everything visible to anyone who can't already see it */
	for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if (ent->inuse) {
			G_AppearPerishEvent(~G_VisToPM(ent->visflags), qtrue, ent, NULL);
			if (G_IsActor(ent))
				G_SendInventory(~G_TeamToPM(ent->team), ent);
		}

	/* send results */
	gi.AddEvent(PM_ALL, EV_RESULTS);
	gi.WriteByte(MAX_TEAMS);
	gi.WriteByte(team);

	for (i = 0; i < MAX_TEAMS; i++) {
		gi.WriteByte(level.num_spawned[i]);
		gi.WriteByte(level.num_alive[i]);
	}

	for (i = 0; i < MAX_TEAMS; i++)
		for (j = 0; j < MAX_TEAMS; j++)
			gi.WriteByte(level.num_kills[i][j]);

	for (i = 0; i < MAX_TEAMS; i++)
		for (j = 0; j < MAX_TEAMS; j++)
			gi.WriteByte(level.num_stuns[i][j]);

	/* how many actors */
	for (j = 0, i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
		if (ent->inuse && G_IsActor(ent) && !G_IsAI(ent))
			j++;

	/* number of soldiers */
	gi.WriteByte(j);

	if (j) {
		for (i = 0, ent = g_edicts; i < globals.num_edicts; ent++, i++)
			if (ent->inuse && G_IsActor(ent) && !G_IsAI(ent)) {
				G_SendCharacterData(ent);
			}
	}

	gi.EndEvents();

	/* now we cleanup the AI */
	AIL_Cleanup();

	if (level.nextmap[0] != '\0') {
		char command[MAX_VAR];
		/** @todo We have to make sure, that the teaminfo is not completely resent
		 * otherwise we would have the same team again and died actors are not taken
		 * into account */
		Com_sprintf(command, sizeof(command), "map %s %s\n",
				level.day ? "day" : "night", level.nextmap);
		gi.AddCommandString(command);
	}
}

/**
 * @brief Checks whether a match is over.
 * @return @c true if this match is over, @c false otherwise
 */
qboolean G_MatchDoEnd (void)
{
	/* check for intermission */
	if (level.intermissionTime && level.time > level.intermissionTime) {
		G_PrintStats(va("End of game - Team %i is the winner", level.winningTeam));
		G_MatchSendResults(level.winningTeam);
		level.intermissionTime = 0.0;
		level.winningTeam = 0;
		return qtrue;
	}

	return qfalse;
}

/**
 * @brief Checks whether there are still actors to fight with left. If non
 * are the match end will be triggered.
 * @sa G_MatchEndTrigger
 */
void G_MatchEndCheck (void)
{
	int activeTeams;
	int i, last;

	if (level.intermissionTime) /* already decided */
		return;

	if (!level.numplayers) {
		G_MatchEndTrigger(0, 0);
		return;
	}

	/** @todo count from 0 to get the civilians for objectives */
	for (i = 1, activeTeams = 0, last = 0; i < MAX_TEAMS; i++)
		if (level.num_alive[i]) {
			last = i;
			activeTeams++;
		}

	/** @todo < 2 does not work when we count civilians */
	/* prepare for sending results */
	if (activeTeams < 2) {
		const int timeGap = (level.activeTeam == TEAM_ALIEN ? 10.0 : 3.0);
		G_MatchEndTrigger(activeTeams == 1 ? last : 0, timeGap);
	}
}

/**
 * @brief Checks whether the game is running (active team and no intermission time)
 * @returns true if there is an active team for the current round and the end of the game
 * was not yet triggered
 * @sa G_MatchEndTrigger
 */
qboolean G_MatchIsRunning (void)
{
	if (level.intermissionTime)
		return qfalse;
	return (level.activeTeam != TEAM_NO_ACTIVE);
}
