/**
 * @file
 */

/*
Copyright (C) 2002-2017 UFO: Alien Invasion Team.

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

#include "e_event_message.h"
#include "../../../cl_hud.h"
#include <string>

void CL_DisplayNetMessage (const eventRegister_t* self, dbuffer* msg)
{
	char buff[MAX_SVC_PRINT];
	size_t len = 0;
	for (;;) {
		int c = NET_ReadByte(msg);
		if (c == -1 || c == 0)
			break;
		if (len < MAX_SVC_PRINT - 1) {
			buff[len] = c;
			++len;
		}
	}
	buff[len] = '\0';

	const char* const str = _(buff);
	const char* const strEnd = str + strlen(str);
	std::string message = "";
	for (const char* c = str; c < strEnd; ++c) {
		if (*c != '%') {
			message.push_back(*c);
		} else if (*++c == '%') {
			message.append("%%");
		} else {
			std::string fmt = "%";
			while (strchr("-+ #0", *c)) fmt.push_back(*c++); /* Flags */
			while (isdigit(*c)) fmt.push_back(*c++); /* Width */
			if (*c == '.') { /* Prescision */
				fmt.push_back(*c++);
				while (isdigit(*c))	fmt.push_back(*c++);
			}
			fmt.push_back(*c);
			char s[MAX_SVC_PRINT];
			switch (*c) {
				case 'c': {
					int t = NET_ReadChar(msg);
					snprintf(s, MAX_SVC_PRINT, fmt.c_str(), t);
					break;
				}
				case 's': {
					char t[MAX_SVC_PRINT];
					NET_ReadString(msg, t, MAX_SVC_PRINT);
					snprintf(s, MAX_SVC_PRINT, fmt.c_str(), _(t));
					break;
				}
				case 'd': case 'i': {
					int t = NET_ReadLong(msg);
					snprintf(s, MAX_SVC_PRINT, fmt.c_str(), t);
					break;
				}
				case 'f': case 'F':
				case 'e': case 'E':
				case 'g': case 'G': {
					float t = NET_ReadAngle(msg);
					snprintf(s, MAX_SVC_PRINT, fmt.c_str(), t);
					break;
				}
				default:
					Com_Error(ERR_DROP, "CL_DisplayNetMessage: invalid format specifier '%s'", fmt.c_str());
					break;
			}
			message.append(s);
		}
	}

	HUD_DisplayMessage(message.c_str());
}