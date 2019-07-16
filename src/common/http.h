/**
 * @file
 */

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

#pragma once

#include "common.h"
#ifndef NO_HTTP
#ifndef CURL_STATICLIB
#define CURL_STATICLIB
#endif
#include <curl/curl.h>

typedef enum {
	DLQ_STATE_NOT_STARTED,
	DLQ_STATE_RUNNING,
	DLQ_STATE_DONE
} dlq_state;

typedef struct dlqueue_s {
	struct dlqueue_s*	next;
	char				ufoPath[MAX_QPATH];
	dlq_state			state;
} dlqueue_t;

typedef struct dlhandle_s {
	CURL*		curl;
	char		filePath[MAX_OSPATH];
	FILE*		file;
	dlqueue_t*	queueEntry;
	size_t		fileSize;
	size_t		position;
	double		speed;
	char		URL[576];
	char*		tempBuffer;
} dlhandle_t;
#endif

typedef struct upparam_s {
	const char* name;
	const char* value;
	struct upparam_s* next;
} upparam_t;

typedef void (*http_callback_t) (const char* response, void* userdata);

bool HTTP_Encode(const char* url, char* out, size_t outLength);
bool HTTP_GetToFile(const char* url, FILE* file, const char* postfields = nullptr);
bool HTTP_GetURL(const char* url, http_callback_t callback, void* userdata = nullptr, const char* postfields = nullptr);
bool HTTP_PutFile(const char* formName, const char* fileName, const char* url, const upparam_t* params);
size_t HTTP_Recv(void* ptr, size_t size, size_t nmemb, void* stream);
size_t HTTP_Header(void* ptr, size_t size, size_t nmemb, void* stream);
void HTTP_Cleanup(void);
bool HTTP_ExtractComponents(const char* url, char* scheme, size_t schemeLength, char* host, size_t hostLength, char* path, size_t pathLength, int* port);
