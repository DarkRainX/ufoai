/*
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

#include <windows.h>
#include <float.h>
#include "../../renderer/r_local.h"
#include "win_ref_glw.h"

int (WINAPI * qwglChoosePixelFormat )(HDC, CONST PIXELFORMATDESCRIPTOR *);
int (WINAPI * qwglDescribePixelFormat) (HDC, int, UINT, LPPIXELFORMATDESCRIPTOR);
BOOL (WINAPI * qwglSetPixelFormat)(HDC, int, CONST PIXELFORMATDESCRIPTOR *);
BOOL (WINAPI * qwglSwapBuffers)(HDC);
HGLRC (WINAPI * qwglCreateContext)(HDC);
BOOL (WINAPI * qwglDeleteContext)(HGLRC);
PROC (WINAPI * qwglGetProcAddress)(LPCSTR);
BOOL (WINAPI * qwglMakeCurrent)(HDC, HGLRC);
BOOL (WINAPI * qwglSwapIntervalEXT)(int interval);

/**
 * @brief Unloads the specified DLL then nulls out all the proc pointers.
 * @sa QR_UnLink
 */
void QR_Shutdown (void)
{
	if (glw_state.hinstOpenGL) {
		FreeLibrary(glw_state.hinstOpenGL);
	}

	glw_state.hinstOpenGL = NULL;

	/* general pointers */
	QR_UnLink();
	/* windows specific */
	qwglCreateContext            = NULL;
	qwglDeleteContext            = NULL;
	qwglGetProcAddress           = NULL;
	qwglMakeCurrent              = NULL;
	qwglChoosePixelFormat        = NULL;
	qwglDescribePixelFormat      = NULL;
	qwglSetPixelFormat           = NULL;
	qwglSwapBuffers              = NULL;
	qwglSwapIntervalEXT          = NULL;
}


/**
 * @brief This is responsible for binding our qgl function pointers to the appropriate GL stuff.
 * @sa QR_Init
 */
qboolean QR_Init (const char *dllname)
{
	if ((glw_state.hinstOpenGL = LoadLibrary(dllname)) == 0) {
		char *buf = NULL;

		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &buf, 0, NULL);
		Com_Printf("%s\n", buf);
		return qfalse;
	}

	/* general qgl bindings */
	QR_Link();
	/* windows specific ones */
	qwglCreateContext            = GPA("wglCreateContext");
	qwglDeleteContext            = GPA("wglDeleteContext");
	qwglGetProcAddress           = GPA("wglGetProcAddress");
	qwglMakeCurrent              = GPA("wglMakeCurrent");
	qwglChoosePixelFormat        = GPA("wglChoosePixelFormat");
	qwglDescribePixelFormat      = GPA("wglDescribePixelFormat");
	qwglSetPixelFormat           = GPA("wglSetPixelFormat");
	qwglSwapBuffers              = GPA("wglSwapBuffers");
	qwglSwapIntervalEXT          = NULL;

	return qtrue;
}
