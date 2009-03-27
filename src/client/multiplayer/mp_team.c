/**
 * @file mp_team.c
 * @brief Multiplayer team management.
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

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "../client.h"
#include "../cl_game.h"
#include "../cl_inventory.h"
#include "../cl_team.h"
#include "../cl_menu.h"
#include "../menu/m_popup.h"
#include "../menu/m_nodes.h"	/**< menuInventory */
#include "mp_team.h"

#define MPTEAM_SAVE_FILE_VERSION 2

static inventory_t mp_inventory;
character_t multiplayerCharacters[MAX_MULTIPLAYER_CHARACTERS];

typedef struct mpSaveFileHeader_s {
	int version; /**< which savegame version */
	int soldiercount; /** the number of soldiers stored in this savegame */
	int compressed; /**< is this file compressed via zlib, currently not implemented */
	char name[32]; /**< savefile name */
	int dummy[14]; /**< maybe we have to extend this later */
	long xml_size; /** needed, if we store compressed */
} mpSaveFileHeader_t;

/**
 * @brief Reads tha comments from team files
 */
void MP_MultiplayerTeamSlotComments_f (void)
{
	int i;

	for (i = 0; i < 8; i++) {
		/* open file */
		qFILE f;

		FS_OpenFile(va("save/team%i.mpt", i), &f, FILE_READ);
		if (f.f || f.z) {
			int version, soldierCount;
			const char *comment;
			mpSaveFileHeader_t header;
			const int clen = sizeof(header);
			if (FS_Read(&header, clen, &f) != clen) {
				Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
				FS_CloseFile(&f);
				continue;
			}
			FS_CloseFile(&f);
			version = header.version;
			comment = header.name;
			soldierCount = header.soldiercount;
			MN_ExecuteConfunc("set_slotname %i %i \"%s\"", i, soldierCount, comment);
		}
	}
}

/**
 * @brief Stores the wholeTeam into a xml structure
 * @note Called by MP_SaveTeamMultiplayerXML to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
static void MP_SaveTeamMultiplayerInfoXML (mxml_node_t *p)
{
	int i;

	/* header */
	Com_DPrintf(DEBUG_CLIENT, "Saving %i teammembers\n", chrDisplayList.num);
	for (i = 0; i < chrDisplayList.num; i++) {
		const character_t *chr = chrDisplayList.chr[i];
		mxml_node_t *n = mxml_AddNode(p, "character");
		CL_SaveCharacterXML(n, *chr);
	}
}

/**
 * @brief Loads the wholeTeam from the xml file
 * @note Called by MP_SaveTeamMultiplayer to store the team info
 * @sa GAME_SendCurrentTeamSpawningInfo
 */
static void MP_LoadTeamMultiplayerInfoXML (mxml_node_t *p)
{
	int i;
	mxml_node_t *n;

	/* header */
	for (i = 0, n = mxml_GetNode(p, "character"); n && i < MAX_MULTIPLAYER_CHARACTERS; i++, n = mxml_GetNextNode(n, p, "character")) {
		CL_LoadCharacterXML(n, &multiplayerCharacters[i]);
		chrDisplayList.chr[i] = &multiplayerCharacters[i];
	}
	chrDisplayList.num = i;
	Com_DPrintf(DEBUG_CLIENT, "Loaded %i teammembers\n", chrDisplayList.num);
}

/**
 * @brief Saves a multiplayer team in xml format
 * @sa MP_SaveTeamMultiplayerInfoXML
 * @sa MP_LoadTeamMultiplayerXML
 */
static qboolean MP_SaveTeamMultiplayerXML (const char *filename, const char *name)
{
	int res;
	int requiredbuflen;
	byte *buf, *fbuf;
	uLongf bufLen;
	mpSaveFileHeader_t header;
	char dummy[2];
	int i;
	mxml_node_t *top_node, *node, *snode;
	equipDef_t *ed = GAME_GetEquipmentDefinition();

	top_node = mxmlNewXML("1.0");
	node = mxml_AddNode(top_node, "multiplayer");
	memset(&header, 0, sizeof(header));
	header.compressed = 0;
	header.version = LittleLong(MPTEAM_SAVE_FILE_VERSION);
	header.soldiercount = LittleLong(chrDisplayList.num);
	Q_strncpyz(header.name, name, sizeof(header.name));

	snode = mxml_AddNode(node, "team");
	MP_SaveTeamMultiplayerInfoXML(snode);

	snode = mxml_AddNode(node, "equipment");
	for (i = 0; i < csi.numODs; i++) {
		mxml_node_t *ssnode = mxml_AddNode(snode, "emission");
		mxml_AddString(ssnode, "id", csi.ods[i].id);
		mxml_AddInt(ssnode, "num", ed->num[i]);
		mxml_AddInt(ssnode, "numloose", ed->numLoose[i]);
	}
	requiredbuflen = mxmlSaveString(top_node, dummy, 2, MXML_NO_CALLBACK);
	/* required for storing compressed */
	header.xml_size = LittleLong(requiredbuflen);

	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * requiredbuflen + 1, cl_genericPool, 0);
	if (!buf) {
		Com_Printf("Error: Could not allocate enough memory to save this game\n");
		return qfalse;
	}
	res = mxmlSaveString(top_node, (char*)buf, requiredbuflen + 1, MXML_NO_CALLBACK);
	Com_Printf("XML Written to buffer (%d Bytes)\n", res);

	bufLen = (uLongf) (24 + 1.02 * requiredbuflen);
	fbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * bufLen + sizeof(header), cl_genericPool, 0);
	memcpy(fbuf, &header, sizeof(header));

	if (header.compressed) {
		res = compress(fbuf + sizeof(header), &bufLen, buf, requiredbuflen + 1);
		Mem_Free(buf);
		if (res != Z_OK) {
			Mem_Free(fbuf);
			Com_Printf("Memory error compressing save-game data (Error: %i)!\n", res);
			return qfalse;
		}
	} else {
		memcpy(fbuf + sizeof(header), buf, requiredbuflen + 1);
		Mem_Free(buf);
	}
	/* last step - write data */
	res = FS_WriteFile(fbuf, bufLen + sizeof(header), filename);
	Mem_Free(fbuf);
	return qtrue;
}

/**
 * @brief Stores a team in a specified teamslot (multiplayer)
 */
void MP_SaveTeamMultiplayer_f (void)
{
	if (!chrDisplayList.num) {
		MN_Popup(_("Note"), _("Error saving team. Nothing to save yet."));
		return;
	} else {
		char filename[MAX_OSPATH];
		int index;
		const char *name;

		if (Cmd_Argc() != 2) {
			Com_Printf("Usage: %s <slotindex>\n", Cmd_Argv(0));
			return;
		}

		index = atoi(Cmd_Argv(1));

		name = Cvar_GetString(va("mn_slot%i", index));
		if (!strlen(name))
			name = _("NewTeam");

		/* save */
		Com_sprintf(filename, sizeof(filename), "save/team%i.mpt", index);
		if (!MP_SaveTeamMultiplayerXML(filename, name))
			MN_Popup(_("Note"), _("Error saving team. Check free disk space!"));
	}
}

/**
 * @brief Load a multiplayer team from an xml file
 * @sa MP_LoadTeamMultiplayer
 * @sa MP_SaveTeamMultiplayer
 */
static qboolean MP_LoadTeamMultiplayerXML (const char *filename)
{
	uLongf len;
	qFILE f;
	byte *cbuf, *buf;
	int i, clen;
	mxml_node_t *top_node, *node, *snode, *ssnode;
	mpSaveFileHeader_t header;
	equipDef_t *ed;

	/* open file */
	FS_OpenFile(filename, &f, FILE_READ);
	if (!f.f && !f.z) {
		Com_Printf("Couldn't open file '%s'\n", filename);
		return qfalse;
	}

	clen = FS_FileLength(&f);
	cbuf = (byte *) Mem_PoolAlloc(sizeof(byte) * clen, cl_genericPool, 0);
	if (FS_Read(cbuf, clen, &f) != clen) {
		Com_Printf("Warning: Could not read %i bytes from savefile\n", clen);
		FS_CloseFile(&f);
		Mem_Free(cbuf);
		return qfalse;
	}
	FS_CloseFile(&f);

	memcpy(&header, cbuf, sizeof(header));
	/* swap all int values if needed */
	header.compressed = LittleLong(header.compressed);
	header.version = LittleLong(header.version);
	header.xml_size = LittleLong(header.xml_size);
	len = header.xml_size + 1 + sizeof(header);

	Com_Printf("Loading multiplayer team (size %d / %li)\n", clen, header.xml_size);

	buf = (byte *) Mem_PoolAlloc(sizeof(byte) * len, cl_genericPool, 0);

	if (header.compressed) {
		/* uncompress data, skipping comment header */
		const int res = uncompress(buf, &len, cbuf + sizeof(header), clen - sizeof(header));

		if (res != Z_OK) {
			Mem_Free(buf);
			Com_Printf("Error decompressing data in '%s'.\n", filename);
			return qfalse;
		}
		top_node = mxmlLoadString(NULL, (char*)buf , mxml_ufo_type_cb);
		if (!top_node) {
			Mem_Free(buf);
			Com_Printf("Error: Failure in loading the team data!");
			return qfalse;
		}
	} else {
		top_node = mxmlLoadString(NULL, (char*)(cbuf + sizeof(header)) , mxml_ufo_type_cb);
		Mem_Free(cbuf);
		if (!top_node) {
			Mem_Free(buf);
			Com_Printf("Error: Failure in Loading the xml Data!");
			return qfalse;
		}
	}

	node = mxml_GetNode(top_node, "multiplayer");
	if (!node) {
		Com_Printf("Error: Failure in Loading the xml Data! (savegame node not found)");
		Mem_Free(buf);
		return qfalse;
	}
	Cvar_Set("mn_teamname", header.name);

	snode = mxml_GetNode(node, "team");
	if (!snode)
		Com_Printf("Team Node not found\n");
	MP_LoadTeamMultiplayerInfoXML(snode);

	snode = mxml_GetNode(node, "equipment");
	if (!snode)
		Com_Printf("Equipment Node not found\n");

	ed = GAME_GetEquipmentDefinition();
	for (i = 0, ssnode = mxml_GetNode(snode, "emission"); ssnode && i < csi.numODs; i++, ssnode = mxml_GetNextNode(ssnode, snode, "emission")) {
		const char *objID = mxml_GetString(ssnode, "id");
		const objDef_t *od = INVSH_GetItemByID(objID);
		if (od) {
			ed->num[od->idx] = mxml_GetInt(snode, "num", 0);
			ed->numLoose[od->idx] = mxml_GetInt(snode, "numloose", 0);
		}
	}

	Com_Printf("File '%s' loaded.\n", filename);
	Mem_Free(buf);
	return qtrue;
}

/**
 * @brief Loads the selected teamslot
 */
void MP_LoadTeamMultiplayer_f (void)
{
	char filename[MAX_OSPATH];
	int index;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s <slotindex>\n", Cmd_Argv(0));
		return;
	}

	index = atoi(Cmd_Argv(1));

	/* first try to load the xml file, if this does not succeed, try the old file */
	Com_sprintf(filename, sizeof(filename), "save/team%i.mpt", index);
	if (!MP_LoadTeamMultiplayerXML(filename))
		Com_Printf("Could not load team '%s'.\n", filename);
	else
		Com_Printf("Team '%s' loaded.\n", filename);
}

/**
 * @brief Get the equipment definition (from script files) for the current selected multiplayer team
 * and updates the equipment inventory containers
 */
static void MP_GetEquipment (void)
{
	const equipDef_t *edFromScript;
	const char *teamID = Com_ValueToStr(&cl_team->integer, V_TEAM, 0);
	char equipmentName[MAX_VAR];
	equipDef_t unused;
	equipDef_t *ed;

	Com_sprintf(equipmentName, sizeof(equipmentName), "multiplayer_%s", teamID);

	/* search equipment definition */
	edFromScript = INV_GetEquipmentDefinitionByID(equipmentName);
	if (edFromScript == NULL) {
		Com_Printf("Equipment '%s' not found!\n", equipmentName);
		return;
	}

	if (chrDisplayList.num > 0)
		menuInventory = &chrDisplayList.chr[0]->inv;
	else
		menuInventory = NULL;

	ed = GAME_GetEquipmentDefinition();
	*ed = *edFromScript;

	/* we don't want to lose anything from ed - so we copy it and screw the copied stuff afterwards */
	unused = *edFromScript;
	/* manage inventory */
	MN_ContainerNodeUpdateEquipment(&mp_inventory, &unused);
}

/**
 * @brief Displays actor info and equipment and unused items in proper (filter) category.
 * @note This function is called everytime the multiplayer equipment screen for the team pops up.
 */
void MP_UpdateMenuParameters_f (void)
{
	int i;

	/* reset description */
	Cvar_Set("mn_itemname", "");
	Cvar_Set("mn_item", "");
	MN_ResetData(TEXT_STANDARD);

	for (i = 0; i < MAX_MULTIPLAYER_CHARACTERS; i++) {
		const char *name;
		if (i < chrDisplayList.num)
			name = chrDisplayList.chr[i]->name;
		else
			name = "";
		Cvar_ForceSet(va("mn_name%i", i), name);
	}

	MP_GetEquipment();
}

void MP_TeamSelect_f (void)
{
	const int old = cl_selected->integer;
	int num;

	/* check syntax */
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <num>\n", Cmd_Argv(0));
		return;
	}

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= chrDisplayList.num)
		return;

	MN_ExecuteConfunc("mp_soldier_select %i", num);
	MN_ExecuteConfunc("mp_soldier_unselect %i", old);
	Cmd_ExecuteString(va("equip_select %i", num));
}
