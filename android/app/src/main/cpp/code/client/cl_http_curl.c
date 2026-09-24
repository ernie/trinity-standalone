/*
===========================================================================
Copyright (C) 2006 Tony J. White (tjw@tjw.org)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_HTTP

#include "client.h"

#ifdef USE_LOCAL_HEADERS
  #include "../curl-8.11.0/include/curl/curl.h"
#else
  #include <curl/curl.h>
#endif

#ifdef USE_CURL_DLOPEN

#ifdef __APPLE__
  #define DEFAULT_CURL_LIB "libcurl.dylib"
#elif defined(__ANDROID__)
  #define DEFAULT_CURL_LIB "libcurl.so"
#else
  #define DEFAULT_CURL_LIB "libcurl.so.4"
  #define ALTERNATE_CURL_LIB "libcurl.so.3"
#endif

#include "../sys/sys_loadlib.h"

cvar_t *cl_cURLLib;

char* (*qcurl_version)(void);

CURL* (*qcurl_easy_init)(void);
CURLcode (*qcurl_easy_setopt)(CURL *curl, CURLoption option, ...);
CURLcode (*qcurl_easy_perform)(CURL *curl);
void (*qcurl_easy_cleanup)(CURL *curl);
CURLcode (*qcurl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
CURL* (*qcurl_easy_duphandle)(CURL *curl);
void (*qcurl_easy_reset)(CURL *curl);
const char *(*qcurl_easy_strerror)(CURLcode);

CURLM* (*qcurl_multi_init)(void);
CURLMcode (*qcurl_multi_add_handle)(CURLM *multi_handle,
                                                CURL *curl_handle);
CURLMcode (*qcurl_multi_remove_handle)(CURLM *multi_handle,
                                                CURL *curl_handle);
CURLMcode (*qcurl_multi_fdset)(CURLM *multi_handle,
                                                fd_set *read_fd_set,
                                                fd_set *write_fd_set,
                                                fd_set *exc_fd_set,
                                                int *max_fd);
CURLMcode (*qcurl_multi_perform)(CURLM *multi_handle,
                                                int *running_handles);
CURLMcode (*qcurl_multi_cleanup)(CURLM *multi_handle);
CURLMsg *(*qcurl_multi_info_read)(CURLM *multi_handle,
                                                int *msgs_in_queue);
const char *(*qcurl_multi_strerror)(CURLMcode);

struct curl_slist *(*qcurl_slist_append)(struct curl_slist *list, const char *data);
void (*qcurl_slist_free_all)(struct curl_slist *list);

static void *cURLLib = NULL;
static qboolean cURLSymbolLoadFailed = qfalse;

#else /* linked at build time */

#define qcurl_version curl_version
#define qcurl_easy_init curl_easy_init
#define qcurl_easy_setopt curl_easy_setopt
#define qcurl_easy_perform curl_easy_perform
#define qcurl_easy_cleanup curl_easy_cleanup
#define qcurl_easy_getinfo curl_easy_getinfo
#define qcurl_easy_duphandle curl_easy_duphandle
#define qcurl_easy_reset curl_easy_reset
#define qcurl_easy_strerror curl_easy_strerror
#define qcurl_multi_init curl_multi_init
#define qcurl_multi_add_handle curl_multi_add_handle
#define qcurl_multi_remove_handle curl_multi_remove_handle
#define qcurl_multi_fdset curl_multi_fdset
#define qcurl_multi_perform curl_multi_perform
#define qcurl_multi_cleanup curl_multi_cleanup
#define qcurl_multi_info_read curl_multi_info_read
#define qcurl_multi_strerror curl_multi_strerror
#define qcurl_slist_append curl_slist_append
#define qcurl_slist_free_all curl_slist_free_all

#endif /* USE_CURL_DLOPEN */

static CURL *downloadCURL = NULL;
static CURLM *downloadCURLM = NULL;

static CURL *inMemoryDownloadCURL = NULL;
static CURLM *inMemoryDownloadCURLM = NULL;
static unsigned char* hInMemoryBuffer = NULL;
static size_t hInMemoryBufferSize = 0;
static size_t hInMemoryBufferCursor = 0;
static CL_HTTP_InMemoryDownloadCallback hInMemoryCallback = NULL;

#ifdef USE_CURL_DLOPEN

/*
=================
GPA
=================
*/
static void *GPA(char *str)
{
	void *rv;

	rv = Sys_LoadFunction(cURLLib, str);
	if(!rv)
	{
		Com_Printf("Can't load symbol %s\n", str);
		cURLSymbolLoadFailed = qtrue;
		return NULL;
	}
	else
	{
		Com_DPrintf("Loaded symbol %s (0x%p)\n", str, rv);
        return rv;
	}
}

/*
=================
CL_HTTP_Init
=================
*/
qboolean CL_HTTP_Init(void)
{
	if(cURLLib)
		return qtrue;

	cl_cURLLib = Cvar_Get("cl_cURLLib", DEFAULT_CURL_LIB, CVAR_ARCHIVE | CVAR_PROTECTED);

	Com_Printf("Loading \"%s\"...", cl_cURLLib->string);
	if(!(cURLLib = Sys_LoadDll(cl_cURLLib->string, qtrue)))
	{
#ifdef ALTERNATE_CURL_LIB
		// On some linux distributions there is no libcurl.so.3, but only libcurl.so.4. That one works too.
		if(!(cURLLib = Sys_LoadDll(ALTERNATE_CURL_LIB, qtrue)))
#endif
		{
			Com_Printf("FAIL\n");
			return qfalse;
		}
	}

	cURLSymbolLoadFailed = qfalse;

	qcurl_version = GPA("curl_version");

	qcurl_easy_init = GPA("curl_easy_init");
	qcurl_easy_setopt = GPA("curl_easy_setopt");
	qcurl_easy_perform = GPA("curl_easy_perform");
	qcurl_easy_cleanup = GPA("curl_easy_cleanup");
	qcurl_easy_getinfo = GPA("curl_easy_getinfo");
	qcurl_easy_duphandle = GPA("curl_easy_duphandle");
	qcurl_easy_reset = GPA("curl_easy_reset");
	qcurl_easy_strerror = GPA("curl_easy_strerror");

	qcurl_multi_init = GPA("curl_multi_init");
	qcurl_multi_add_handle = GPA("curl_multi_add_handle");
	qcurl_multi_remove_handle = GPA("curl_multi_remove_handle");
	qcurl_multi_fdset = GPA("curl_multi_fdset");
	qcurl_multi_perform = GPA("curl_multi_perform");
	qcurl_multi_cleanup = GPA("curl_multi_cleanup");
	qcurl_multi_info_read = GPA("curl_multi_info_read");
	qcurl_multi_strerror = GPA("curl_multi_strerror");

	qcurl_slist_append = GPA("curl_slist_append");
	qcurl_slist_free_all = GPA("curl_slist_free_all");

	if(cURLSymbolLoadFailed)
	{
		CL_HTTP_Shutdown();
		Com_Printf("FAIL One or more symbols not found\n");
		return qfalse;
	}
	Com_Printf("OK\n");

	return qtrue;
}

/*
=================
CL_HTTP_Available
=================
*/
qboolean CL_HTTP_Available(void)
{
	return cURLLib != NULL;
}

#else /* linked at build time */

/*
=================
CL_HTTP_Init
=================
*/
qboolean CL_HTTP_Init(void)
{
	Com_Printf("cURL %s\n", qcurl_version());
	return qtrue;
}

/*
=================
CL_HTTP_Available
=================
*/
qboolean CL_HTTP_Available(void)
{
	return qtrue;
}

#endif /* USE_CURL_DLOPEN */

static void CL_cURL_Cleanup(void)
{
	if(downloadCURLM) {
		CURLMcode result;

		if(downloadCURL) {
			result = qcurl_multi_remove_handle(downloadCURLM,
				downloadCURL);
			if(result != CURLM_OK) {
				Com_DPrintf("qcurl_multi_remove_handle failed: %s\n", qcurl_multi_strerror(result));
			}
			qcurl_easy_cleanup(downloadCURL);
		}
		result = qcurl_multi_cleanup(downloadCURLM);
		if(result != CURLM_OK) {
			Com_DPrintf("CL_cURL_Cleanup: qcurl_multi_cleanup failed: %s\n", qcurl_multi_strerror(result));
		}
		downloadCURLM = NULL;
		downloadCURL = NULL;
	}
	else if(downloadCURL) {
		qcurl_easy_cleanup(downloadCURL);
		downloadCURL = NULL;
	}
}

static void CL_cURL_InMemoryCleanup(void)
{
	if(inMemoryDownloadCURLM) {
		CURLMcode result;

		if(inMemoryDownloadCURL) {
			result = qcurl_multi_remove_handle(inMemoryDownloadCURLM, inMemoryDownloadCURL);
			if(result != CURLM_OK) {
				Com_DPrintf("qcurl_multi_remove_handle failed: %s\n", qcurl_multi_strerror(result));
			}
			qcurl_easy_cleanup(inMemoryDownloadCURL);
		}
		result = qcurl_multi_cleanup(inMemoryDownloadCURLM);
		if(result != CURLM_OK) {
			Com_DPrintf("CL_cURL_Cleanup: qcurl_multi_cleanup failed: %s\n", qcurl_multi_strerror(result));
		}
		inMemoryDownloadCURLM = NULL;
		inMemoryDownloadCURL = NULL;
	}
	else if(inMemoryDownloadCURL) {
		qcurl_easy_cleanup(inMemoryDownloadCURL);
		inMemoryDownloadCURL = NULL;
	}
}

/*
=================
CL_HTTP_Shutdown
=================
*/
void CL_HTTP_Shutdown( void )
{
	CL_cURL_Cleanup();

#ifdef USE_CURL_DLOPEN
	if(cURLLib)
	{
		Sys_UnloadLibrary(cURLLib);
		cURLLib = NULL;
	}
	qcurl_easy_init = NULL;
	qcurl_easy_setopt = NULL;
	qcurl_easy_perform = NULL;
	qcurl_easy_cleanup = NULL;
	qcurl_easy_getinfo = NULL;
	qcurl_easy_duphandle = NULL;
	qcurl_easy_reset = NULL;

	qcurl_multi_init = NULL;
	qcurl_multi_add_handle = NULL;
	qcurl_multi_remove_handle = NULL;
	qcurl_multi_fdset = NULL;
	qcurl_multi_perform = NULL;
	qcurl_multi_cleanup = NULL;
	qcurl_multi_info_read = NULL;
	qcurl_multi_strerror = NULL;

	qcurl_slist_append = NULL;
	qcurl_slist_free_all = NULL;
#endif
}

static int CL_cURL_CallbackProgress(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
    curl_off_t ultotal, curl_off_t ulnow)
{
	clc.downloadSize = (int)dltotal;
	Cvar_SetValue( "cl_downloadSize", clc.downloadSize );
	clc.downloadCount = (int)dlnow;
	Cvar_SetValue( "cl_downloadCount", clc.downloadCount );
	return 0;
}

static size_t CL_cURL_CallbackWrite(void *buffer, size_t size, size_t nmemb,
	void *stream)
{
	FS_Write( buffer, size*nmemb, ((fileHandle_t*)stream)[0] );
	return size*nmemb;
}

static size_t CL_cURL_InMemoryCallbackWrite(void *buffer, size_t size, size_t nmemb,
	void *stream)
{
	if (hInMemoryBufferSize < size*nmemb)
	{
		return 0;
	}

	memcpy(hInMemoryBuffer + hInMemoryBufferCursor, buffer, size*nmemb);
	hInMemoryBufferCursor += size*nmemb;
	hInMemoryBufferSize -= size*nmemb;
	return size*nmemb;
}

CURLcode qcurl_easy_setopt_warn(CURL *curl, CURLoption option, ...)
{
	CURLcode result;

	va_list argp;
	va_start(argp, option);

	if(option < CURLOPTTYPE_OBJECTPOINT) {
		long longValue = va_arg(argp, long);
		result = qcurl_easy_setopt(curl, option, longValue);
	} else if(option < CURLOPTTYPE_OFF_T) {
		void *pointerValue = va_arg(argp, void *);
		result = qcurl_easy_setopt(curl, option, pointerValue);
	} else {
		curl_off_t offsetValue = va_arg(argp, curl_off_t);
		result = qcurl_easy_setopt(curl, option, offsetValue);
	}

	if(result != CURLE_OK) {
		Com_DPrintf("qcurl_easy_setopt failed: %s\n", qcurl_easy_strerror(result));
	}
	va_end(argp);

	return result;
}

void CL_HTTP_BeginDownload( const char *remoteURL )
{
	CURLMcode result;

	CL_cURL_Cleanup();

	downloadCURL = qcurl_easy_init();
	if(!downloadCURL) {
		Com_Error(ERR_DROP, "CL_HTTP_BeginDownload: qcurl_easy_init() "
			"failed");
		return;
	}

	if(com_developer->integer)
		qcurl_easy_setopt_warn(downloadCURL, CURLOPT_VERBOSE, 1);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_URL, remoteURL);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_TRANSFERTEXT, 0);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_REFERER, va("ioQ3://%s",
		NET_AdrToString(clc.serverAddress)));
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_USERAGENT, va("%s %s",
		Q3_VERSION, qcurl_version()));
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_WRITEFUNCTION,
		CL_cURL_CallbackWrite);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_WRITEDATA, &clc.download);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_NOPROGRESS, 0);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_XFERINFOFUNCTION,
		CL_cURL_CallbackProgress);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_PROGRESSDATA, NULL);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_FAILONERROR, 1);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_FOLLOWLOCATION, 1);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_MAXREDIRS, 5);
#if CURL_AT_LEAST_VERSION(7,85,0)
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_PROTOCOLS_STR, "http,https");
#else
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
	qcurl_easy_setopt_warn(downloadCURL, CURLOPT_CAINFO, va( "%s/cacert.pem", Com_TrinityHome() ));
	downloadCURLM = qcurl_multi_init();
	if(!downloadCURLM) {
		qcurl_easy_cleanup(downloadCURL);
		downloadCURL = NULL;
		Com_Error(ERR_DROP, "CL_HTTP_BeginDownload: qcurl_multi_init() "
			"failed");
		return;
	}
	result = qcurl_multi_add_handle(downloadCURLM, downloadCURL);
	if(result != CURLM_OK) {
		qcurl_easy_cleanup(downloadCURL);
		downloadCURL = NULL;
		Com_Error(ERR_DROP,"CL_HTTP_BeginDownload: qcurl_multi_add_handle() failed: %s", qcurl_multi_strerror(result));
		return;
	}
}

qboolean CL_HTTP_PerformDownload(void)
{
	CURLMcode res;
	CURLMsg *msg;
	int c;
	int i = 0;

	res = qcurl_multi_perform(downloadCURLM, &c);
	while(res == CURLM_CALL_MULTI_PERFORM && i < 100) {
		res = qcurl_multi_perform(downloadCURLM, &c);
		i++;
	}
	if(res == CURLM_CALL_MULTI_PERFORM)
		return qfalse;
	msg = qcurl_multi_info_read(downloadCURLM, &c);
	if(msg == NULL) {
		return qfalse;
	}
	if(msg->msg != CURLMSG_DONE || msg->data.result != CURLE_OK) {
		long code;

		qcurl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE,
			&code);	
		Com_Error(ERR_DROP, "Download Error: %s Code: %ld URL: %s",
			qcurl_easy_strerror(msg->data.result),
			code, clc.downloadURL);
	}

	return qtrue;
}

void CL_HTTP_BeginInMemoryDownload( const char *remoteURL, CL_HTTP_InMemoryDownloadCallback callback, unsigned char* buffer, size_t bufferSize )
{
	CURLMcode result;

	CL_cURL_InMemoryCleanup();

	inMemoryDownloadCURL = qcurl_easy_init();
	if(!inMemoryDownloadCURL) {
		Com_Error(ERR_DROP, "CL_HTTP_BeginDownload: qcurl_easy_init() "
			"failed");
		return;
	}

	if(com_developer->integer)
		qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_VERBOSE, 1);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_URL, remoteURL);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_TRANSFERTEXT, 0);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_USERAGENT, va("%s %s",
		Q3_VERSION, qcurl_version()));
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_WRITEFUNCTION,
		CL_cURL_InMemoryCallbackWrite);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_NOPROGRESS, 1);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_FAILONERROR, 1);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_FOLLOWLOCATION, 1);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_MAXREDIRS, 5);
#if CURL_AT_LEAST_VERSION(7,85,0)
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_PROTOCOLS_STR, "http,https");
#else
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
	qcurl_easy_setopt_warn(inMemoryDownloadCURL, CURLOPT_CAINFO, va( "%s/cacert.pem", Com_TrinityHome() ));

	inMemoryDownloadCURLM = qcurl_multi_init();
	if(!inMemoryDownloadCURLM) {
		qcurl_easy_cleanup(inMemoryDownloadCURL);
		inMemoryDownloadCURL = NULL;
		fprintf(stderr, "CL_HTTP_BeginInMemoryDownload: qcurl_multi_init() failed");
		return;
	}

	result = qcurl_multi_add_handle(inMemoryDownloadCURLM, inMemoryDownloadCURL);
	if(result != CURLM_OK) {
		qcurl_easy_cleanup(inMemoryDownloadCURL);
		inMemoryDownloadCURL = NULL;
		fprintf(stderr, "CL_HTTP_BeginDownload: qcurl_multi_add_handle() failed: %s", qcurl_multi_strerror(result));
		return;
	}

	hInMemoryBuffer = buffer;
	hInMemoryBufferSize = bufferSize;
	hInMemoryBufferCursor = 0;
	hInMemoryCallback = callback;
}

qboolean CL_HTTP_PerformInMemoryDownload(void)
{
	CURLMcode res;
	CURLMsg *msg;
	int c;
	int i = 0;

	if(!inMemoryDownloadCURLM)
		return qfalse;

	res = qcurl_multi_perform(inMemoryDownloadCURLM, &c);
	while(res == CURLM_CALL_MULTI_PERFORM && i < 100) {
		res = qcurl_multi_perform(inMemoryDownloadCURLM, &c);
		i++;
	}
	if(res == CURLM_CALL_MULTI_PERFORM)
		return qfalse;
	msg = qcurl_multi_info_read(inMemoryDownloadCURLM, &c);
	if(msg == NULL) {
		return qfalse;
	}
	if(msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK) {
		if(hInMemoryCallback)
			hInMemoryCallback(hInMemoryBufferCursor);
	} else {
		long code;

		qcurl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE,
			&code);
		Com_Printf(S_COLOR_RED "In-memory download error: %s Code: %ld\n",
			qcurl_easy_strerror(msg->data.result), code);
		if(hInMemoryCallback)
			hInMemoryCallback(-1);
	}

	CL_cURL_InMemoryCleanup();

	hInMemoryCallback = NULL;
	hInMemoryBuffer = NULL;
	hInMemoryBufferSize = 0;
	hInMemoryBufferCursor = 0;

	return qtrue;
}

/*
=================
TV Demo Download

Standalone HTTP download for TV demo files.
Uses dedicated cURL handles so it doesn't interfere
with pk3 downloads.
=================
*/

static CURL *tvDownloadCURL = NULL;
static CURLM *tvDownloadCURLM = NULL;
static fileHandle_t tvFile = 0;
static char tvLocalName[MAX_QPATH];
static char tvTempName[MAX_QPATH];

static size_t CL_cURL_TV_CallbackWrite( void *buffer, size_t size, size_t nmemb, void *stream )
{
	FS_Write( buffer, size * nmemb, *(fileHandle_t *)stream );
	return size * nmemb;
}

static int CL_cURL_TV_CallbackProgress( void *clientp, curl_off_t dltotal, curl_off_t dlnow,
	curl_off_t ultotal, curl_off_t ulnow )
{
	Cvar_SetIntegerValue( "cl_downloadSize", (int)dltotal );
	Cvar_SetIntegerValue( "cl_downloadCount", (int)dlnow );
	return 0;
}

static void CL_cURL_TV_Cleanup( void )
{
	if ( tvDownloadCURLM ) {
		CURLMcode result;

		if ( tvDownloadCURL ) {
			result = qcurl_multi_remove_handle( tvDownloadCURLM, tvDownloadCURL );
			if ( result != CURLM_OK ) {
				Com_DPrintf( "TV: qcurl_multi_remove_handle failed: %s\n", qcurl_multi_strerror( result ) );
			}
			qcurl_easy_cleanup( tvDownloadCURL );
		}
		result = qcurl_multi_cleanup( tvDownloadCURLM );
		if ( result != CURLM_OK ) {
			Com_DPrintf( "TV: qcurl_multi_cleanup failed: %s\n", qcurl_multi_strerror( result ) );
		}
		tvDownloadCURLM = NULL;
		tvDownloadCURL = NULL;
	} else if ( tvDownloadCURL ) {
		qcurl_easy_cleanup( tvDownloadCURL );
		tvDownloadCURL = NULL;
	}
}

qboolean CL_HTTP_TV_BeginDownload( const char *localName, const char *remoteURL )
{
	CURLMcode result;

	if ( !CL_HTTP_Available() ) {
		Com_Printf( S_COLOR_YELLOW "TV: cURL not available\n" );
		return qfalse;
	}

	CL_HTTP_TV_CleanupDownload();

	Q_strncpyz( tvLocalName, localName, sizeof( tvLocalName ) );
	Com_sprintf( tvTempName, sizeof( tvTempName ), "%s.tmp", localName );

	tvFile = FS_FOpenFileWrite( tvTempName );
	if ( !tvFile ) {
		Com_Printf( S_COLOR_RED "TV: could not open %s for writing\n", tvTempName );
		return qfalse;
	}

	tvDownloadCURL = qcurl_easy_init();
	if ( !tvDownloadCURL ) {
		Com_Printf( S_COLOR_RED "TV: qcurl_easy_init() failed\n" );
		FS_FCloseFile( tvFile );
		tvFile = 0;
		FS_HomeRemove( tvTempName );
		return qfalse;
	}

	Cvar_Set( "cl_downloadName", tvLocalName );
	Cvar_Set( "cl_downloadSize", "0" );
	Cvar_Set( "cl_downloadCount", "0" );
	Cvar_SetIntegerValue( "cl_downloadTime", cls.realtime );

	if ( com_developer->integer )
		qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_VERBOSE, 1 );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_URL, remoteURL );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_TRANSFERTEXT, 0 );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_USERAGENT, va( "%s %s", Q3_VERSION, qcurl_version() ) );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_WRITEFUNCTION, CL_cURL_TV_CallbackWrite );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_WRITEDATA, &tvFile );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_NOPROGRESS, 0 );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_XFERINFOFUNCTION, CL_cURL_TV_CallbackProgress );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_XFERINFODATA, NULL );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_FAILONERROR, 1 );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_FOLLOWLOCATION, 1 );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_MAXREDIRS, 5 );
#if CURL_AT_LEAST_VERSION(7,85,0)
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_PROTOCOLS_STR, "http,https" );
#else
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS );
#endif
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE );
	qcurl_easy_setopt_warn( tvDownloadCURL, CURLOPT_CAINFO, va( "%s/cacert.pem", Com_TrinityHome() ) );

	tvDownloadCURLM = qcurl_multi_init();
	if ( !tvDownloadCURLM ) {
		CL_cURL_TV_Cleanup();
		FS_FCloseFile( tvFile );
		tvFile = 0;
		FS_HomeRemove( tvTempName );
		Com_Printf( S_COLOR_RED "TV: qcurl_multi_init() failed\n" );
		return qfalse;
	}

	result = qcurl_multi_add_handle( tvDownloadCURLM, tvDownloadCURL );
	if ( result != CURLM_OK ) {
		CL_cURL_TV_Cleanup();
		FS_FCloseFile( tvFile );
		tvFile = 0;
		FS_HomeRemove( tvTempName );
		Com_Printf( S_COLOR_RED "TV: qcurl_multi_add_handle() failed: %s\n", qcurl_multi_strerror( result ) );
		return qfalse;
	}

	return qtrue;
}

qboolean CL_HTTP_TV_PerformDownload( void )
{
	CURLMcode res;
	CURLMsg *msg;
	int c;
	int i = 0;

	if ( !tvDownloadCURLM ) {
		return qtrue;
	}

	res = qcurl_multi_perform( tvDownloadCURLM, &c );
	while ( res == CURLM_CALL_MULTI_PERFORM && i < 100 ) {
		res = qcurl_multi_perform( tvDownloadCURLM, &c );
		i++;
	}
	if ( res == CURLM_CALL_MULTI_PERFORM ) {
		return qfalse;
	}

	msg = qcurl_multi_info_read( tvDownloadCURLM, &c );
	if ( msg == NULL ) {
		return qfalse;
	}

	FS_FCloseFile( tvFile );
	tvFile = 0;

	if ( msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK ) {
		FS_Rename( tvTempName, tvLocalName );
		Com_Printf( S_COLOR_GREEN "Downloaded TVD: %s\n", tvLocalName );
	} else {
		long code;

		qcurl_easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &code );
		Com_Printf( S_COLOR_RED "TVD download error: %s Code: %ld\n",
			qcurl_easy_strerror( msg->data.result ), code );
		FS_HomeRemove( tvTempName );
	}

	Cvar_Set( "cl_downloadName", "" );
	Cvar_Set( "cl_downloadSize", "0" );
	Cvar_Set( "cl_downloadCount", "0" );
	Cvar_Set( "cl_downloadTime", "0" );

	CL_cURL_TV_Cleanup();
	tvLocalName[0] = '\0';
	tvTempName[0] = '\0';

	return qtrue;
}

void CL_HTTP_TV_CleanupDownload( void )
{
	CL_cURL_TV_Cleanup();

	if ( tvFile ) {
		FS_FCloseFile( tvFile );
		tvFile = 0;
	}

	if ( tvTempName[0] ) {
		FS_HomeRemove( tvTempName );
		tvTempName[0] = '\0';
	}

	tvLocalName[0] = '\0';
}

#endif /* USE_HTTP */
