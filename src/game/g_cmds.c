/**
 * @file g_cmds.c
 * @brief Player commands.
 */

/*
All original material Copyright (C) 2002-2007 UFO: Alien Invasion team.

Original file from Quake 2 v3.21: quake2-2.31/game/g_cmds.c
Copyright (C) 1997-2001 Id Software, Inc.

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
#include "../shared/parse.h"

static void G_Players_f (const player_t *player)
{
	int i;
	int count = 0;
	char smallBuf[64];
	char largeBuf[1280];

	/* print information */
	largeBuf[0] = 0;

	for (i = 0; i < game.sv_maxplayersperteam; i++) {
		const player_t *p = &game.players[i];
		if (!p->inuse)
			continue;

		Com_sprintf(smallBuf, sizeof(smallBuf), "(%i) Team %i %s status: %s\n", p->num,
				p->pers.team, p->pers.netname, (p->ready ? "waiting" : "playing"));

		/* can't print all of them in one packet */
		if (strlen(smallBuf) + strlen(largeBuf) > sizeof(largeBuf) - 100) {
			Q_strcat(largeBuf, "...\n", sizeof(largeBuf));
			break;
		}
		Q_strcat(largeBuf, smallBuf, sizeof(largeBuf));
		count++;
	}

	gi.PlayerPrintf(player, PRINT_CONSOLE, "%s\n%i players\n", largeBuf, count);
}

/**
 * @brief Check whether the user can talk
 */
static qboolean G_CheckFlood (player_t *player)
{
	int i;

	if (flood_msgs->integer) {
		if (level.time < player->pers.flood_locktill) {
			gi.PlayerPrintf(player, PRINT_CONSOLE, "You can't talk for %d more seconds\n",
					   (int)(player->pers.flood_locktill - level.time));
			return qtrue;
		}
		i = player->pers.flood_whenhead - flood_msgs->value + 1;
		if (i < 0)
			i = (sizeof(player->pers.flood_when)/sizeof(player->pers.flood_when[0])) + i;
		if (player->pers.flood_when[i] && level.time - player->pers.flood_when[i] < flood_persecond->value) {
			player->pers.flood_locktill = level.time + flood_waitdelay->value;
			gi.PlayerPrintf(player, PRINT_CHAT, "Flood protection: You can't talk for %d seconds.\n",
					   flood_waitdelay->integer);
			return qtrue;
		}
		player->pers.flood_whenhead = (player->pers.flood_whenhead + 1) %
				(sizeof(player->pers.flood_when)/sizeof(player->pers.flood_when[0]));
		player->pers.flood_when[player->pers.flood_whenhead] = level.time;
	}
	return qfalse;
}

static void G_Say_f (player_t *player, qboolean arg0, qboolean team)
{
	int j;
	const char *p, *token;
	char text[2048];

	if (gi.Cmd_Argc() < 2 && !arg0)
		return;

	if (G_CheckFlood(player))
		return;

	if (!team)
		Com_sprintf(text, sizeof(text), "%s: ", player->pers.netname);
	else
		Com_sprintf(text, sizeof(text), "^B%s (team): ", player->pers.netname);

	if (arg0) {
		Q_strcat(text, gi.Cmd_Argv(0), sizeof(text));
		Q_strcat(text, " ", sizeof(text));
		Q_strcat(text, gi.Cmd_Args(), sizeof(text));
	} else {
		p = gi.Cmd_Args();

		token = COM_Parse(&p);

		Q_strcat(text, token, sizeof(text));
	}

	/* don't let text be too long for malicious reasons */
	if (strlen(text) > 150)
		text[150] = 0;

	Q_strcat(text, "\n", sizeof(text));

	if (sv_dedicated->integer)
		gi.dprintf("%s", text);

	for (j = 0; j < game.sv_maxplayersperteam; j++) {
		if (!game.players[j].inuse)
			continue;
		if (team && game.players[j].pers.team != player->pers.team)
			continue;
		gi.PlayerPrintf(&game.players[j], PRINT_CHAT, "%s", text);
	}
}

#ifdef DEBUG
/**
 * @brief This function does not add statistical values. Because there is no attacker.
 * The same counts for morale states - they are not affected.
 * @note: This is a debug function to let a hole team die
 */
static void G_KillTeam_f (void)
{
	/* default is to kill all teams */
	int teamToKill = -1, i;
	edict_t *ent;

	/* with a parameter we will be able to kill a specific team */
	if (gi.Cmd_Argc() == 2)
		teamToKill = atoi(gi.Cmd_Argv(1));

	Com_DPrintf(DEBUG_GAME, "G_KillTeam: kill team %i\n", teamToKill);

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent)) {
			if (teamToKill >= 0 && ent->team != teamToKill)
				continue;

			/* die */
			G_ActorDie(ent, STATE_DEAD, NULL);
		}

	/* check for win conditions */
	G_CheckEndGame();
}

/**
 * @brief Stun all members of a giben team.
 */
static void G_StunTeam_f (void)
{
	/* default is to kill all teams */
	int teamToKill = -1, i;
	edict_t *ent;

	/* with a parameter we will be able to kill a specific team */
	if (gi.Cmd_Argc() == 2)
		teamToKill = atoi(gi.Cmd_Argv(1));

	Com_DPrintf(DEBUG_GAME, "G_StunTeam: stun team %i\n", teamToKill);

	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent)) {
			if (teamToKill >= 0 && ent->team != teamToKill)
				continue;

			/* die */
			G_ActorDie(ent, STATE_STUN, NULL);

			if (teamToKill == TEAM_ALIEN)
				level.num_stuns[TEAM_PHALANX][TEAM_ALIEN]++;
			else
				level.num_stuns[TEAM_ALIEN][teamToKill]++;
		}

	/* check for win conditions */
	G_CheckEndGame();
}

/**
 * @brief Prints all mission-score entries of all team members.
 * @note Console command: debug_listscore
 */
static void G_ListMissionScore_f (void)
{
	int team = -1;
	edict_t *ent;
	int entIdx, i, j;

	/* With a parameter we will be able to get the info for a specific team */
	if (gi.Cmd_Argc() == 2) {
		team = atoi(gi.Cmd_Argv(1));
	} else {
		Com_Printf("Usage: %s <teamnumber>\n", gi.Cmd_Argv(0));
		return;
	}

	for (entIdx = 0, ent = g_edicts; entIdx < globals.num_edicts; entIdx++, ent++) {
		if (ent->inuse && G_IsLivingActor(ent)) {
			if (team >= 0 && ent->team != team)
				continue;

			assert(ent->chr.scoreMission);

			Com_Printf("Soldier: %s\n", ent->chr.name);

			/* ===================== */
			Com_Printf("  Move: Normal=%i Crouched=%i\n", ent->chr.scoreMission->movedNormal, ent->chr.scoreMission->movedCrouched);

			Com_Printf("  Kills:");
			for (i = 0; i < KILLED_NUM_TYPES; i++) {
				Com_Printf(" %i", ent->chr.scoreMission->kills[i]);
			}
			Com_Printf("\n");

			Com_Printf("  Stuns:");
			for (i = 0; i < KILLED_NUM_TYPES; i++) {
				Com_Printf(" %i", ent->chr.scoreMission->stuns[i]);
			}
			Com_Printf("\n");

			/* ===================== */
			Com_Printf("  Fired:");
			for (i = 0; i < SKILL_NUM_TYPES; i++) {
				Com_Printf(" %i", ent->chr.scoreMission->fired[i]);
			}
			Com_Printf("\n");

			Com_Printf("  Hits:\n");
			for (i = 0; i < SKILL_NUM_TYPES; i++) {
				Com_Printf("    Skill%i: ",i);
				for (j = 0; j < KILLED_NUM_TYPES; j++) {
					Com_Printf(" %i", ent->chr.scoreMission->hits[i][j]);
				}
				Com_Printf("\n");
			}

			/* ===================== */
			Com_Printf("  Fired Splash:");
			for (i = 0; i < SKILL_NUM_TYPES; i++) {
				Com_Printf(" %i", ent->chr.scoreMission->firedSplash[i]);
			}
			Com_Printf("\n");

			Com_Printf("  Hits Splash:\n");
			for (i = 0; i < SKILL_NUM_TYPES; i++) {
				Com_Printf("    Skill%i: ",i);
				for (j = 0; j < KILLED_NUM_TYPES; j++) {
					Com_Printf(" %i", ent->chr.scoreMission->hitsSplash[i][j]);
				}
				Com_Printf("\n");
			}

			Com_Printf("  Splash Damage:\n");
			for (i = 0; i < SKILL_NUM_TYPES; i++) {
				Com_Printf("    Skill%i: ",i);
				for (j = 0; j < KILLED_NUM_TYPES; j++) {
					Com_Printf(" %i", ent->chr.scoreMission->hitsSplashDamage[i][j]);
				}
				Com_Printf("\n");
			}

			/* ===================== */
			Com_Printf("  Kills per skill:");
			for (i = 0; i < SKILL_NUM_TYPES; i++) {
				Com_Printf(" %i", ent->chr.scoreMission->skillKills[i]);
			}
			Com_Printf("\n");

			/* ===================== */
			Com_Printf("  Heal (received): %i\n", ent->chr.scoreMission->heal);
		}
	}
}

/**
 * @brief Debug function to print a player's inventory
 */
void G_InvList_f (const player_t *player)
{
	edict_t *ent;
	int i;

	Com_Printf("Print inventory for '%s'\n", player->pers.netname);
	for (i = 0, ent = g_edicts; i < globals.num_edicts; i++, ent++)
		if (ent->inuse && G_IsLivingActor(ent) && ent->team == player->pers.team) {
			Com_Printf("actor: '%s'\n", ent->chr.name);
			INVSH_PrintContainerToConsole(&ent->i);
		}
}

static void G_TouchEdict_f (void)
{
	edict_t *e;
	int i, j;

	if (gi.Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	i = atoi(gi.Cmd_Argv(1));
	if (i < 0 || i >= globals.num_edicts)
		return;

	e = &g_edicts[i];
	if (!e->touch) {
		Com_Printf("No touch function for entity %s\n", e->classname);
		return;
	}

	for (j = 0; j < globals.num_edicts; j++)
		if (G_IsLivingActor(&g_edicts[j]))
			break;
	if (j == globals.num_edicts)
		return;

	Com_Printf("Call touch function for %s\n", e->classname);
	e->touch(e, &g_edicts[j]);
}

static void G_UseEdict_f (void)
{
	edict_t *e;
	int i;

	if (gi.Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <entnum>\n", gi.Cmd_Argv(0));
		return;
	}

	i = atoi(gi.Cmd_Argv(1));
	if (i < 0 || i >= globals.num_edicts)
		return;

	e = &g_edicts[i];
	if (!e->use) {
		Com_Printf("No use function for entity %s\n", e->classname);
		return;
	}

	Com_Printf("Call use function for %s\n", e->classname);
	e->use(e);
}

#endif

void G_ClientCommand (player_t * player)
{
	const char *cmd;

	if (!player->inuse)
		return;					/* not fully in game yet */

	cmd = gi.Cmd_Argv(0);

	if (Q_strcasecmp(cmd, "players") == 0)
		G_Players_f(player);
	else if (Q_strcasecmp(cmd, "say") == 0)
		G_Say_f(player, qfalse, qfalse);
	else if (Q_strcasecmp(cmd, "say_team") == 0)
		G_Say_f(player, qfalse, qtrue);
#ifdef DEBUG
	else if (Q_strcasecmp(cmd, "debug_actorinvlist") == 0)
		G_InvList_f(player);
	else if (Q_strcasecmp(cmd, "debug_killteam") == 0)
		G_KillTeam_f();
	else if (Q_strcasecmp(cmd, "debug_stunteam") == 0)
		G_StunTeam_f();
	else if (Q_strcasecmp(cmd, "debug_listscore") == 0)
		G_ListMissionScore_f();
	else if (Q_strcasecmp(cmd, "debug_edicttouch") == 0)
		G_TouchEdict_f();
	else if (Q_strcasecmp(cmd, "debug_edictuse") == 0)
		G_UseEdict_f();
#endif
	else
		/* anything that doesn't match a command will be a chat */
		G_Say_f(player, qtrue, qfalse);
}
