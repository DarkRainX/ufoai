/**
 * @file
 * @brief Serverlist menu callbacks for multiplayer
 */

/*
Copyright (C) 2002-2013 UFO: Alien Invasion.

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

#include "../../cl_shared.h"
#include "../../ui/ui_data.h"
#include "mp_callbacks.h"
#include "mp_serverlist.h"
#include "../cl_game.h"

static const cgame_import_t *cgi;

teamData_t teamData;
cvar_t *rcon_client_password;
cvar_t *cl_maxsoldiersperteam;
cvar_t *cl_maxsoldiersperplayer;
static cvar_t *rcon_address;
static cvar_t *info_password;

static void CL_Connect_f (void)
{
	char server[MAX_VAR];
	char serverport[16];

	if (!selectedServer && cgi->Cmd_Argc() != 2 && cgi->Cmd_Argc() != 3) {
		cgi->Com_Printf("Usage: %s <server> [<port>]\n", cgi->Cmd_Argv(0));
		return;
	}

	if (cgi->Cmd_Argc() == 2) {
		Q_strncpyz(server, cgi->Cmd_Argv(1), sizeof(server));
		Q_strncpyz(serverport, DOUBLEQUOTE(PORT_SERVER), sizeof(serverport));
	} else if (cgi->Cmd_Argc() == 3) {
		Q_strncpyz(server, cgi->Cmd_Argv(1), sizeof(server));
		Q_strncpyz(serverport, cgi->Cmd_Argv(2), sizeof(serverport));
	} else {
		assert(selectedServer);
		Q_strncpyz(server, selectedServer->node, sizeof(server));
		Q_strncpyz(serverport, selectedServer->service, sizeof(serverport));
	}

	if (cgi->GAME_IsTeamEmpty() && !cgi->GAME_LoadDefaultTeam(true)) {
		cgi->UI_Popup(_("Error"), "%s", _("Assemble a team first"));
		return;
	}

	if (cgi->Cvar_GetInteger("mn_server_need_password")) {
		cgi->UI_PushWindow("serverpassword");
		return;
	}

	/* if running a local server, kill it and reissue */
	cgi->SV_Shutdown("Server quit.", false);
	cgi->CL_Disconnect();

	cgi->GAME_SetServerInfo(server, serverport);

	cgi->CL_SetClientState(ca_connecting);

	cgi->HUD_InitUI("multiplayerInGame");
}

static void CL_RconCallback (struct net_stream *s)
{
	dbuffer *buf = cgi->NET_ReadMsg(s);
	if (buf) {
		const int cmd = cgi->NET_ReadByte(buf);
		char commandBuf[8];
		cgi->NET_ReadStringLine(buf, commandBuf, sizeof(commandBuf));

		if (cmd == svc_oob && Q_streq(commandBuf, "print")) {
			char paramBuf[2048];
			cgi->NET_ReadString(buf, paramBuf, sizeof(paramBuf));
			cgi->Com_Printf("%s\n", paramBuf);
		}
		delete buf;
	}
	cgi->NET_StreamFree(s);
}

bool MP_Rcon (const char *password, const char *command)
{
	if (Q_strnull(password)) {
		cgi->Com_Printf("You must set 'rcon_password' before issuing a rcon command.\n");
		return false;
	}

	if (cgi->CL_GetClientState() >= ca_connected) {
		cgi->NET_OOB_Printf2("rcon %s %s", password, command);
		return true;
	} else if (rcon_address->string) {
		const char *port;

		if (strstr(rcon_address->string, ":"))
			port = strstr(rcon_address->string, ":") + 1;
		else
			port = DOUBLEQUOTE(PORT_SERVER);

		struct net_stream *s = cgi->NET_Connect(rcon_address->string, port, nullptr);
		if (s) {
			cgi->NET_OOB_Printf(s, "rcon %s %s", password, command);
			cgi->NET_StreamSetCallback(s, &CL_RconCallback);
			return true;
		}
	}

	cgi->Com_Printf("You are not connected to any server\n");
	return false;
}
/**
 * Send the rest of the command line over as
 * an unconnected command.
 */
static void CL_Rcon_f (void)
{
	if (cgi->Cmd_Argc() < 2) {
		cgi->Com_Printf("Usage: %s <command>\n", cgi->Cmd_Argv(0));
		return;
	}

	if (!rcon_client_password->string) {
		cgi->Com_Printf("You must set 'rcon_password' before issuing a rcon command.\n");
		return;
	}

	if (!MP_Rcon(rcon_client_password->string, cgi->Cmd_Args()))
		Com_Printf("Could not send the rcon command");
}

static void CL_StartGame_f (void)
{
	if (cgi->Com_ServerState())
		cgi->Cmd_ExecuteString("startgame");
	else
		cgi->Cmd_ExecuteString("rcon startgame");
}

/**
 * @brief Binding for disconnect console command
 * @sa CL_Disconnect
 * @sa CL_Drop
 * @sa SV_ShutdownWhenEmpty
 */
static void CL_Disconnect_f (void)
{
	cgi->SV_ShutdownWhenEmpty();
	cgi->CL_Drop();
}

/**
 * @brief The server is changing levels
 */
static void CL_Reconnect_f (void)
{
	if (cgi->Com_ServerState())
		return;

	if (cgi->CL_GetClientState() >= ca_connecting) {
		cgi->Com_Printf("disconnecting...\n");
		cgi->CL_Disconnect();
	}

	cgi->CL_SetClientState(ca_connecting);
	cgi->Com_Printf("reconnecting...\n");
}

/**
 * @brief Send the teaminfo string to server
 * @sa CL_ParseTeamInfoMessage
 * @sa SV_ConnectionlessPacket
 */
static void CL_SelectTeam_Init_f (void)
{
	/* reset menu text */
	cgi->UI_ResetData(TEXT_STANDARD);

	if (cgi->Com_ServerState())
		cgi->Cvar_Set("cl_admin", "1");
	else
		cgi->Cvar_Set("cl_admin", "0");

	cgi->NET_OOB_Printf2("teaminfo %i", PROTOCOL_VERSION);
	cgi->UI_RegisterText(TEXT_STANDARD, _("Select a free team or your coop team"));
}

static bool GAME_MP_SetTeamNum (int teamnum)
{
	if (teamData.maxPlayersPerTeam > teamData.teamCount[teamnum]) {
		static char buf[MAX_VAR];
		cgi->Cvar_SetValue("cl_teamnum", teamnum);
		Com_sprintf(buf, sizeof(buf), _("Current team: %i"), teamnum);
		cgi->UI_RegisterText(TEXT_STANDARD, buf);
		return true;
	}

	cgi->UI_RegisterText(TEXT_STANDARD, _("Team is already in use"));
	cgi->Com_DPrintf(DEBUG_CLIENT, "team %i is already in use: %i (max: %i)\n",
		teamnum, teamData.teamCount[teamnum], teamData.maxPlayersPerTeam);
	return false;
}

/**
 * @brief Increase or decrease the teamnum
 * @sa CL_SelectTeam_Init_f
 */
static void CL_TeamNum_f (void)
{
	int i = cgi->Cvar_GetInteger("cl_teamnum");

	if (i <= TEAM_CIVILIAN || i > teamData.maxteams) {
		cgi->Cvar_SetValue("cl_teamnum", TEAM_DEFAULT);
		i = TEAM_DEFAULT;
	}

	if (Q_streq(cgi->Cmd_Argv(0), "teamnum_dec")) {
		for (i--; i > TEAM_CIVILIAN; i--) {
			if (GAME_MP_SetTeamNum(i))
				break;
		}
	} else {
		for (i++; i <= teamData.maxteams; i++) {
			if (GAME_MP_SetTeamNum(i))
				break;
		}
	}

	CL_SelectTeam_Init_f();
}

/**
 * @brief Autocomplete function for some network functions
 * @sa Cmd_AddParamCompleteFunction
 * @todo Extend this for all the servers on the server browser list
 */
static int CL_CompleteNetworkAddress (const char *partial, const char **match)
{
	int n = 0;
	for (int i = 0; i != MAX_BOOKMARKS; ++i) {
		char const* const adrStr = cgi->Cvar_GetString(va("adr%i", i));
		if (adrStr[0] != '\0' && cgi->Cmd_GenericCompleteFunction(adrStr, partial, match)) {
			cgi->Com_Printf("%s\n", adrStr);
			++n;
		}
	}
	return n;
}

void MP_CallbacksInit (const cgame_import_t *import)
{
	cgi = import;
	rcon_client_password = cgi->Cvar_Get("rcon_password", "", 0, "Remote console password");
	rcon_address = cgi->Cvar_Get("rcon_address", "", 0, "Address of the host you would like to control via rcon");
	info_password = cgi->Cvar_Get("password", "", CVAR_USERINFO, nullptr);
	cl_maxsoldiersperteam = cgi->Cvar_Get("sv_maxsoldiersperteam", "4", CVAR_ARCHIVE | CVAR_SERVERINFO, "How many soldiers may one team have");
	cl_maxsoldiersperplayer = cgi->Cvar_Get("sv_maxsoldiersperplayer", STRINGIFY(MAX_ACTIVETEAM), CVAR_ARCHIVE | CVAR_SERVERINFO, "How many soldiers one player is able to control in a given team");
	cgi->Cmd_AddCommand("mp_selectteam_init", CL_SelectTeam_Init_f, "Function that gets all connected players and let you choose a free team");
	cgi->Cmd_AddCommand("teamnum_dec", CL_TeamNum_f, "Decrease the preferred teamnum");
	cgi->Cmd_AddCommand("teamnum_inc", CL_TeamNum_f, "Increase the preferred teamnum");
	cgi->Cmd_AddCommand("pingservers", CL_PingServers_f, "Ping all servers in local network to get the serverlist");
	cgi->Cmd_AddCommand("disconnect", CL_Disconnect_f, "Disconnect from the current server");
	cgi->Cmd_AddCommand("connect", CL_Connect_f, "Connect to given ip");
	cgi->Cmd_AddParamCompleteFunction("connect", CL_CompleteNetworkAddress);
	cgi->Cmd_AddCommand("reconnect", CL_Reconnect_f, "Reconnect to last server");
	cgi->Cmd_AddCommand("rcon", CL_Rcon_f, "Execute a rcon command - see rcon_password");
	cgi->Cmd_AddCommand("cl_startgame", CL_StartGame_f, "Forces a gamestart if you are the admin");
	cgi->Cmd_AddParamCompleteFunction("rcon", CL_CompleteNetworkAddress);

	cl_maxsoldiersperteam->modified = false;
	cl_maxsoldiersperplayer->modified = false;
}

void MP_CallbacksShutdown (void)
{
	cgi->Cmd_RemoveCommand("mp_selectteam_init");
	cgi->Cmd_RemoveCommand("teamnum_dec");
	cgi->Cmd_RemoveCommand("teamnum_inc");
	cgi->Cmd_RemoveCommand("rcon");
	cgi->Cmd_RemoveCommand("pingservers");
	cgi->Cmd_RemoveCommand("disconnect");
	cgi->Cmd_RemoveCommand("connect");
	cgi->Cmd_RemoveCommand("reconnect");

	cgi->Cvar_Delete("rcon_password");
	cgi->Cvar_Delete("rcon_address");
	cgi->Cvar_Delete("password");
}
