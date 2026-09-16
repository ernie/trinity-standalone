// autoupdate.c -- self-update via GitHub Releases (Android APK variant)

#ifdef USE_HTTP

#include "../client/client.h"
#include "autoupdate.h"

#define JSON_IMPLEMENTATION
#include "json.h"

#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

// curl includes
#ifdef USE_LOCAL_HEADERS
  #include "../curl-8.11.0/include/curl/curl.h"
#else
  #include <curl/curl.h>
#endif

// On Android with USE_CURL_DLOPEN, use the qcurl_* wrappers from cl_http_curl.c.
#ifdef USE_CURL_DLOPEN
extern char* (*qcurl_version)(void);
extern CURL* (*qcurl_easy_init)(void);
extern CURLcode (*qcurl_easy_setopt)(CURL *curl, CURLoption option, ...);
extern void (*qcurl_easy_cleanup)(CURL *curl);
extern CURLcode (*qcurl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
extern const char *(*qcurl_easy_strerror)(CURLcode);
extern CURLM* (*qcurl_multi_init)(void);
extern CURLMcode (*qcurl_multi_add_handle)(CURLM *multi_handle, CURL *curl_handle);
extern CURLMcode (*qcurl_multi_remove_handle)(CURLM *multi_handle, CURL *curl_handle);
extern CURLMcode (*qcurl_multi_perform)(CURLM *multi_handle, int *running_handles);
extern CURLMcode (*qcurl_multi_cleanup)(CURLM *multi_handle);
extern CURLMsg *(*qcurl_multi_info_read)(CURLM *multi_handle, int *msgs_in_queue);
#else
#define qcurl_easy_init curl_easy_init
#define qcurl_easy_setopt curl_easy_setopt
#define qcurl_easy_cleanup curl_easy_cleanup
#define qcurl_easy_getinfo curl_easy_getinfo
#define qcurl_easy_strerror curl_easy_strerror
#define qcurl_multi_init curl_multi_init
#define qcurl_multi_add_handle curl_multi_add_handle
#define qcurl_multi_remove_handle curl_multi_remove_handle
#define qcurl_multi_perform curl_multi_perform
#define qcurl_multi_cleanup curl_multi_cleanup
#define qcurl_multi_info_read curl_multi_info_read
#endif

// compile-time defaults (overridden by CMake defines)
#ifndef UPDATE_GITHUB_OWNER
#define UPDATE_GITHUB_OWNER "ernie"
#endif

#ifndef UPDATE_GITHUB_REPO
#define UPDATE_GITHUB_REPO "trinity-standalone"
#endif

#ifndef UPDATE_ASSET_PREFIX
#define UPDATE_ASSET_PREFIX "trinity-standalone"
#endif

#define UPDATE_API_BUFSIZE		(256 * 1024)

// APK download path
#define UPDATE_DIR				"/sdcard/Trinity/.updates"
#define UPDATE_APK_PATH			UPDATE_DIR "/trinity-standalone-update.apk"

// from sys_android.c
extern void Sys_InstallApk( const char *apkPath );

// state
static updateState_t	updateState = UPDATE_IDLE;
static CURL				*updateCURL;
static CURLM			*updateCURLM;
static unsigned char	*apiResponseBuf;
static int				apiResponseLen;

// cvars (set by engine, read by UI)
static cvar_t	*update_available;
static cvar_t	*update_version;
static cvar_t	*update_current;
static cvar_t	*update_size;
static cvar_t	*update_state_cvar;
static cvar_t	*update_progress;
static cvar_t	*update_error;
static cvar_t	*update_check;
static cvar_t	*update_force;

// parsed release info
static char		releaseVersion[64];
static char		releaseAssetURL[MAX_OSPATH];
static int		releaseAssetSize;


/*
==================
Update_CurlCleanup

Clean up curl handles.
==================
*/
static void Update_CurlCleanup( void )
{
	if ( updateCURLM && updateCURL ) {
		qcurl_multi_remove_handle( updateCURLM, updateCURL );
	}
	if ( updateCURL ) {
		qcurl_easy_cleanup( updateCURL );
		updateCURL = NULL;
	}
	if ( updateCURLM ) {
		qcurl_multi_cleanup( updateCURLM );
		updateCURLM = NULL;
	}
}


/*
==================
Update_ParseVersion

Parse "vX.Y.Z" or "X.Y.Z" into major/minor/patch.
Returns 0 on success, -1 on failure.
==================
*/
static int Update_ParseVersion( const char *str, int *major, int *minor, int *patch )
{
	if ( !str || !major || !minor || !patch )
		return -1;

	*major = *minor = *patch = 0;

	if ( *str == 'v' || *str == 'V' )
		str++;

	if ( !isdigit( (unsigned char)*str ) )
		return -1;

	while ( isdigit( (unsigned char)*str ) ) {
		*major = ( *major * 10 ) + ( *str - '0' );
		str++;
	}

	if ( *str == '.' ) {
		str++;
		while ( isdigit( (unsigned char)*str ) ) {
			*minor = ( *minor * 10 ) + ( *str - '0' );
			str++;
		}
	}

	if ( *str == '.' ) {
		str++;
		while ( isdigit( (unsigned char)*str ) ) {
			*patch = ( *patch * 10 ) + ( *str - '0' );
			str++;
		}
	}

	return 0;
}


/*
==================
Update_GetCurrentVersion

Extract version string from com_engine cvar ("trinity-standalone/vX.Y.Z")
==================
*/
static const char *Update_GetCurrentVersion( void )
{
	const char *engine = Cvar_VariableString( "com_engine" );
	const char *slash = strrchr( engine, '/' );
	return slash ? slash + 1 : engine;
}


/*
==================
Update_SetState
==================
*/
static void Update_SetState( updateState_t state )
{
	updateState = state;
	Cvar_SetIntegerValue( "update_state", (int)state );
}


/*
==================
Update_SetError
==================
*/
static void Update_SetError( const char *msg )
{
	Com_Printf( S_COLOR_RED "Update: %s\n", msg );
	Cvar_Set( "update_error", msg );
	Update_SetState( UPDATE_ERROR );
}


/*
==================
Update_CleanupDownload

Remove leftover APK from a previous download.
==================
*/
static void Update_CleanupDownload( void )
{
	struct stat st;
	if ( stat( UPDATE_APK_PATH, &st ) == 0 ) {
		remove( UPDATE_APK_PATH );
		Com_DPrintf( "Update: cleaned up previous download\n" );
	}
}


/*
==================
Update_APIWriteCallback

curl write callback for in-memory API response.
==================
*/
static size_t Update_APIWriteCallback( void *ptr, size_t size, size_t nmemb, void *userdata )
{
	int bytes = (int)( size * nmemb );

	(void)userdata;

	if ( apiResponseLen + bytes >= UPDATE_API_BUFSIZE - 1 ) {
		return 0; // too large, abort
	}

	memcpy( apiResponseBuf + apiResponseLen, ptr, bytes );
	apiResponseLen += bytes;
	apiResponseBuf[apiResponseLen] = '\0';

	return bytes;
}


/*
==================
Update_ParseAPIResponse

Parse GitHub releases/latest JSON response.
Extract tag_name, and find the matching APK asset URL + size.
==================
*/
static qboolean Update_ParseAPIResponse( void )
{
	const char *json, *jsonEnd;
	const char *tagValue, *assetsValue, *assetEntry;
	char tagName[64];
	char assetName[MAX_OSPATH];
	char assetURL[MAX_OSPATH];
	int curMajor, curMinor, curPatch;
	int newMajor, newMinor, newPatch;
	const char *curVer;
	int prefixLen;

	json = (const char *)apiResponseBuf;
	jsonEnd = json + apiResponseLen;

	// extract tag_name
	tagValue = JSON_ObjectGetNamedValue( json, jsonEnd, "tag_name" );
	if ( !tagValue ) {
		Update_SetError( "No tag_name in release response" );
		return qfalse;
	}

	if ( !JSON_ValueGetString( tagValue, jsonEnd, tagName, sizeof( tagName ) ) ) {
		Update_SetError( "Failed to read tag_name" );
		return qfalse;
	}

	Q_strncpyz( releaseVersion, tagName, sizeof( releaseVersion ) );
	Cvar_Set( "update_version", releaseVersion );

	// compare versions
	curVer = Update_GetCurrentVersion();
	Cvar_Set( "update_current", curVer );

	if ( Update_ParseVersion( curVer, &curMajor, &curMinor, &curPatch ) != 0 ) {
		Update_SetError( "Cannot parse current version" );
		return qfalse;
	}
	if ( Update_ParseVersion( tagName, &newMajor, &newMinor, &newPatch ) != 0 ) {
		Update_SetError( "Cannot parse release version" );
		return qfalse;
	}

	if ( !update_force->integer &&
		( newMajor < curMajor ||
		( newMajor == curMajor && newMinor < curMinor ) ||
		( newMajor == curMajor && newMinor == curMinor && newPatch <= curPatch ) ) ) {
		Com_Printf( "Update: already up to date (%s)\n", curVer );
		Cvar_SetIntegerValue( "update_available", 0 );
		Update_SetState( UPDATE_IDLE );
		return qfalse;
	}

	// find the APK asset: match prefix and .apk suffix
	prefixLen = strlen( UPDATE_ASSET_PREFIX );

	assetsValue = JSON_ObjectGetNamedValue( json, jsonEnd, "assets" );
	if ( !assetsValue ) {
		Update_SetError( "No assets in release" );
		return qfalse;
	}

	releaseAssetURL[0] = '\0';
	releaseAssetSize = 0;

	for ( assetEntry = JSON_ArrayGetFirstValue( assetsValue, jsonEnd );
		  assetEntry;
		  assetEntry = JSON_ArrayGetNextValue( assetEntry, jsonEnd ) )
	{
		const char *nameVal = JSON_ObjectGetNamedValue( assetEntry, jsonEnd, "name" );
		const char *urlVal = JSON_ObjectGetNamedValue( assetEntry, jsonEnd, "browser_download_url" );
		const char *sizeVal = JSON_ObjectGetNamedValue( assetEntry, jsonEnd, "size" );
		int nameLen;

		if ( !nameVal || !urlVal )
			continue;

		JSON_ValueGetString( nameVal, jsonEnd, assetName, sizeof( assetName ) );

		// match: starts with prefix and ends with .apk
		nameLen = strlen( assetName );
		if ( nameLen > 4 &&
			 Q_stricmpn( assetName, UPDATE_ASSET_PREFIX, prefixLen ) == 0 &&
			 Q_stricmp( assetName + nameLen - 4, ".apk" ) == 0 ) {
			JSON_ValueGetString( urlVal, jsonEnd, assetURL, sizeof( assetURL ) );
			Q_strncpyz( releaseAssetURL, assetURL, sizeof( releaseAssetURL ) );
			if ( sizeVal )
				releaseAssetSize = JSON_ValueGetInt( sizeVal, jsonEnd );
			break;
		}
	}

	if ( !releaseAssetURL[0] ) {
		Update_SetError( va( "No APK asset found for '%s' in release %s", UPDATE_ASSET_PREFIX, tagName ) );
		return qfalse;
	}

	if ( releaseAssetSize >= 1024 * 1024 )
		Com_Printf( "Update: %s available (current: %s, size: %i.%iMB)\n",
			releaseVersion, curVer,
			releaseAssetSize / (1024*1024),
			(releaseAssetSize / (1024*1024/10)) % 10 );
	else
		Com_Printf( "Update: %s available (current: %s, size: %iKB)\n",
			releaseVersion, curVer, releaseAssetSize / 1024 );

	Cvar_SetIntegerValue( "update_available", 1 );
	Cvar_SetIntegerValue( "update_size", releaseAssetSize );

	return qtrue;
}


/*
==================
Update_BeginCheck

Start async HTTP GET to GitHub releases API.
==================
*/
static void Update_BeginCheck( void )
{
	char apiURL[MAX_OSPATH];

	if ( updateState != UPDATE_IDLE && updateState != UPDATE_ERROR ) {
		Com_Printf( "Update: operation already in progress\n" );
		return;
	}

	Update_CurlCleanup();

	if ( !CL_HTTP_Available() ) {
		Update_SetError( "HTTP not available" );
		return;
	}

	updateCURL = qcurl_easy_init();
	if ( !updateCURL ) {
		Update_SetError( "cURL easy_init failed" );
		return;
	}

	// allocate response buffer
	if ( !apiResponseBuf ) {
		apiResponseBuf = Z_Malloc( UPDATE_API_BUFSIZE );
	}
	apiResponseLen = 0;
	apiResponseBuf[0] = '\0';

	Com_sprintf( apiURL, sizeof( apiURL ),
		"https://api.github.com/repos/%s/%s/releases/latest",
		UPDATE_GITHUB_OWNER, UPDATE_GITHUB_REPO );

	Com_Printf( "Update: checking %s/%s...\n", UPDATE_GITHUB_OWNER, UPDATE_GITHUB_REPO );

	if ( com_developer->integer )
		qcurl_easy_setopt( updateCURL, CURLOPT_VERBOSE, 1 );

	qcurl_easy_setopt( updateCURL, CURLOPT_URL, apiURL );
	qcurl_easy_setopt( updateCURL, CURLOPT_USERAGENT, va( "%s/%s", UPDATE_GITHUB_REPO, Update_GetCurrentVersion() ) );
	qcurl_easy_setopt( updateCURL, CURLOPT_WRITEFUNCTION, Update_APIWriteCallback );
	qcurl_easy_setopt( updateCURL, CURLOPT_WRITEDATA, NULL );
	qcurl_easy_setopt( updateCURL, CURLOPT_NOPROGRESS, 1 );
	qcurl_easy_setopt( updateCURL, CURLOPT_FAILONERROR, 1 );
	qcurl_easy_setopt( updateCURL, CURLOPT_FOLLOWLOCATION, 1 );
	qcurl_easy_setopt( updateCURL, CURLOPT_MAXREDIRS, 5 );
	qcurl_easy_setopt( updateCURL, CURLOPT_TIMEOUT, 30 );
	qcurl_easy_setopt( updateCURL, CURLOPT_CAINFO, "/sdcard/Trinity/cacert.pem" );
#if CURL_AT_LEAST_VERSION(7, 85, 0)
	qcurl_easy_setopt( updateCURL, CURLOPT_PROTOCOLS_STR, "https" );
#else
	qcurl_easy_setopt( updateCURL, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS );
#endif

	updateCURLM = qcurl_multi_init();
	if ( !updateCURLM ) {
		Update_SetError( "cURL multi_init failed" );
		Update_CurlCleanup();
		return;
	}

	if ( qcurl_multi_add_handle( updateCURLM, updateCURL ) != CURLM_OK ) {
		Update_SetError( "cURL multi_add_handle failed" );
		Update_CurlCleanup();
		return;
	}

	Update_SetState( UPDATE_CHECKING );
}


/*
==================
Update_PerformCheck

Poll the API check download. Called from Update_Frame.
==================
*/
static void Update_PerformCheck( void )
{
	CURLMcode res;
	CURLMsg *msg;
	int c, i;

	res = qcurl_multi_perform( updateCURLM, &c );

	i = 0;
	while ( res == CURLM_CALL_MULTI_PERFORM && i < 128 ) {
		res = qcurl_multi_perform( updateCURLM, &c );
		i++;
	}
	if ( res == CURLM_CALL_MULTI_PERFORM )
		return;

	msg = qcurl_multi_info_read( updateCURLM, &c );
	if ( msg == NULL )
		return;

	// done
	if ( msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK ) {
		Update_CurlCleanup();
		if ( Update_ParseAPIResponse() ) {
			Update_SetState( UPDATE_AVAILABLE );
		}
		// else: state was set by ParseAPIResponse (IDLE or ERROR)
	} else {
		long code = 0;
		qcurl_easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &code );
		Update_CurlCleanup();
		if ( code == 403 ) {
			Update_SetError( "GitHub API rate limited. Try again later." );
		} else {
			Update_SetError( va( "Update check failed (HTTP %ld)", code ) );
		}
	}
}


/*
==================
Update_FileWriteCallback

curl write callback for APK download: writes directly to a raw file handle.
==================
*/
static FILE *updateApkFile;

static size_t Update_FileWriteCallback( void *ptr, size_t size, size_t nmemb, void *userdata )
{
	(void)userdata;
	if ( !updateApkFile )
		return 0;
	return fwrite( ptr, size, nmemb, updateApkFile );
}


/*
==================
Update_DownloadProgressCallback

curl progress callback for APK download.
==================
*/
#if CURL_AT_LEAST_VERSION(7, 32, 0)
static int Update_DownloadProgressCallback( void *data, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
#else
static int Update_DownloadProgressCallback( void *data, double dltotal, double dlnow, double ultotal, double ulnow )
#endif
{
	(void)data;
	(void)ultotal;
	(void)ulnow;

	if ( dltotal > 0 ) {
		Cvar_SetIntegerValue( "update_progress", (int)( ( dlnow * 100 ) / dltotal ) );
	}
	Cvar_SetIntegerValue( "update_size", (int)dltotal );

	return 0;
}


/*
==================
Update_BeginDownload

Start downloading the release APK.
==================
*/
static void Update_BeginDownload( void )
{
	if ( !releaseAssetURL[0] ) {
		Update_SetError( "No asset URL" );
		return;
	}

	Update_CurlCleanup();

	if ( !CL_HTTP_Available() ) {
		Update_SetError( "HTTP not available" );
		return;
	}

	updateCURL = qcurl_easy_init();
	if ( !updateCURL ) {
		Update_SetError( "cURL easy_init failed" );
		return;
	}

	// ensure download directory exists
	mkdir( UPDATE_DIR, 0755 );

	updateApkFile = fopen( UPDATE_APK_PATH, "wb" );
	if ( !updateApkFile ) {
		Update_SetError( va( "Cannot write to %s", UPDATE_APK_PATH ) );
		Update_CurlCleanup();
		return;
	}

	Com_Printf( "Update: downloading %s...\n", releaseVersion );

	Cvar_SetIntegerValue( "update_progress", 0 );

	if ( com_developer->integer )
		qcurl_easy_setopt( updateCURL, CURLOPT_VERBOSE, 1 );

	qcurl_easy_setopt( updateCURL, CURLOPT_URL, releaseAssetURL );
	qcurl_easy_setopt( updateCURL, CURLOPT_USERAGENT, va( "%s/%s", UPDATE_GITHUB_REPO, Update_GetCurrentVersion() ) );
	qcurl_easy_setopt( updateCURL, CURLOPT_WRITEFUNCTION, Update_FileWriteCallback );
	qcurl_easy_setopt( updateCURL, CURLOPT_WRITEDATA, NULL );
	qcurl_easy_setopt( updateCURL, CURLOPT_NOPROGRESS, 0 );
#if CURL_AT_LEAST_VERSION(7, 32, 0)
	qcurl_easy_setopt( updateCURL, CURLOPT_XFERINFOFUNCTION, Update_DownloadProgressCallback );
	qcurl_easy_setopt( updateCURL, CURLOPT_XFERINFODATA, NULL );
#else
	qcurl_easy_setopt( updateCURL, CURLOPT_PROGRESSFUNCTION, Update_DownloadProgressCallback );
	qcurl_easy_setopt( updateCURL, CURLOPT_PROGRESSDATA, NULL );
#endif
	qcurl_easy_setopt( updateCURL, CURLOPT_FAILONERROR, 1 );
	qcurl_easy_setopt( updateCURL, CURLOPT_FOLLOWLOCATION, 1 );
	qcurl_easy_setopt( updateCURL, CURLOPT_MAXREDIRS, 10 );
	qcurl_easy_setopt( updateCURL, CURLOPT_CAINFO, "/sdcard/Trinity/cacert.pem" );
#if CURL_AT_LEAST_VERSION(7, 85, 0)
	qcurl_easy_setopt( updateCURL, CURLOPT_PROTOCOLS_STR, "https" );
#else
	qcurl_easy_setopt( updateCURL, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS );
#endif

#ifdef CURL_MAX_READ_SIZE
	qcurl_easy_setopt( updateCURL, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE );
#endif

	updateCURLM = qcurl_multi_init();
	if ( !updateCURLM ) {
		fclose( updateApkFile );
		updateApkFile = NULL;
		Update_SetError( "cURL multi_init failed" );
		Update_CurlCleanup();
		return;
	}

	if ( qcurl_multi_add_handle( updateCURLM, updateCURL ) != CURLM_OK ) {
		fclose( updateApkFile );
		updateApkFile = NULL;
		Update_SetError( "cURL multi_add_handle failed" );
		Update_CurlCleanup();
		return;
	}

	Update_SetState( UPDATE_DOWNLOADING );
}


/*
==================
Update_PerformDownload

Poll the APK download. Called from Update_Frame.
==================
*/
static void Update_PerformDownload( void )
{
	CURLMcode res;
	CURLMsg *msg;
	int c, i;

	res = qcurl_multi_perform( updateCURLM, &c );

	i = 0;
	while ( res == CURLM_CALL_MULTI_PERFORM && i < 128 ) {
		res = qcurl_multi_perform( updateCURLM, &c );
		i++;
	}
	if ( res == CURLM_CALL_MULTI_PERFORM )
		return;

	msg = qcurl_multi_info_read( updateCURLM, &c );
	if ( msg == NULL )
		return;

	// close the file
	if ( updateApkFile ) {
		fclose( updateApkFile );
		updateApkFile = NULL;
	}

	if ( msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK ) {
		Update_CurlCleanup();
		Com_Printf( "Update: download complete. Ready to install.\n" );
		Cvar_SetIntegerValue( "update_progress", 100 );
		Update_SetState( UPDATE_READY );
	} else {
		long code = 0;
		qcurl_easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &code );
		Update_CurlCleanup();
		remove( UPDATE_APK_PATH );
		Update_SetError( va( "Download failed (HTTP %ld)", code ) );
	}
}


/*
==================
Update_Init
==================
*/
void Update_Init( void )
{
	update_available = Cvar_Get( "update_available", "0", CVAR_ROM );
	update_version = Cvar_Get( "update_version", "", CVAR_ROM );
	update_current = Cvar_Get( "update_current", "", CVAR_ROM );
	update_size = Cvar_Get( "update_size", "0", CVAR_ROM );
	update_state_cvar = Cvar_Get( "update_state", "0", CVAR_ROM );
	update_progress = Cvar_Get( "update_progress", "0", CVAR_ROM );
	update_error = Cvar_Get( "update_error", "", CVAR_ROM );
	update_check = Cvar_Get( "update_check", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( update_check, "Check for engine updates on startup." );
	update_force = Cvar_Get( "update_force", "0", CVAR_TEMP );
	Cvar_SetDescription( update_force, "Force update even if current version is equal or newer." );

	Cmd_AddCommand( "update", Update_Check_f );
	Cmd_AddCommand( "updatedownload", Update_Download_f );
	Cmd_AddCommand( "updatecancel", Update_Cancel_f );
	// same command name as trinity-engine/trinity-vr (and what the trinity UI
	// issues); on Android it launches the package installer instead of restarting
	Cmd_AddCommand( "updaterestart", Update_Install_f );

	// clean up leftover APK from a previous download
	Update_CleanupDownload();

	// auto-check on startup if enabled
	if ( update_check->integer ) {
		Update_BeginCheck();
	}
}


/*
==================
Update_Frame

Called once per client frame to poll async operations.
==================
*/
void Update_Frame( void )
{
	switch ( updateState ) {
	case UPDATE_CHECKING:
		Update_PerformCheck();
		break;
	case UPDATE_DOWNLOADING:
		Update_PerformDownload();
		break;
	default:
		break;
	}
}


/*
==================
Update_Shutdown
==================
*/
void Update_Shutdown( void )
{
	if ( updateApkFile ) {
		fclose( updateApkFile );
		updateApkFile = NULL;
	}

	Update_CurlCleanup();

	if ( apiResponseBuf ) {
		Z_Free( apiResponseBuf );
		apiResponseBuf = NULL;
	}

	Cmd_RemoveCommand( "update" );
	Cmd_RemoveCommand( "updatedownload" );
	Cmd_RemoveCommand( "updatecancel" );
	Cmd_RemoveCommand( "updaterestart" );
}


/*
==================
Update_Check_f

Console command: \update
==================
*/
void Update_Check_f( void )
{
	Update_BeginCheck();
}


/*
==================
Update_Download_f

Console command: \updatedownload
==================
*/
void Update_Download_f( void )
{
	if ( updateState != UPDATE_AVAILABLE ) {
		Com_Printf( "Update: no update available to download\n" );
		return;
	}

	Update_BeginDownload();
}


/*
==================
Update_Cancel_f

Console command: \updatecancel
==================
*/
void Update_Cancel_f( void )
{
	if ( updateState != UPDATE_DOWNLOADING )
		return;

	if ( updateApkFile ) {
		fclose( updateApkFile );
		updateApkFile = NULL;
	}
	Update_CurlCleanup();
	remove( UPDATE_APK_PATH );
	Com_Printf( "Update: download cancelled\n" );

	Update_SetState( UPDATE_AVAILABLE );
}


/*
==================
Update_Install_f

Console command: \updaterestart
Launches the Android package installer with the downloaded APK.
==================
*/
void Update_Install_f( void )
{
	struct stat st;

	if ( updateState != UPDATE_READY ) {
		Com_Printf( "Update: no update ready to install\n" );
		return;
	}

	if ( stat( UPDATE_APK_PATH, &st ) != 0 ) {
		Update_SetError( "Downloaded APK not found" );
		return;
	}

	Com_Printf( "Update: launching installer for %s...\n", releaseVersion );
	Sys_InstallApk( UPDATE_APK_PATH );
}


#endif // USE_HTTP
