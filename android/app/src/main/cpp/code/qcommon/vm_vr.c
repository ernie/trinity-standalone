#include "vm_local.h"
#include "vm_vr.h"
#include "vm_vr_select.h"
#include "../vrcommon/vr_shared.h"

// the select's read of the QVM, handed to VM_LoadQVM so the file is read once
static struct {
	vm_t *vm;
	void *buffer;
	int length;
	qboolean sentinel;
} s_read;

/*
==============
VM_VRFallbackPakName

Short display name for a search path: the pk3 or game-directory
basename, short enough for a headset notice.
==============
*/
static const char *VM_VRFallbackPakName( const char *path ) {
	const char *base = path;
	for ( ; *path; path++ ) {
		if ( *path == '/' || *path == '\\' )
			base = path + 1;
	}
	return base;
}

// Trinity's native modules carry the game code of these directories only
static qboolean VM_VRBaseGame( const char *gameDir ) {
	return !gameDir[0] || !Q_stricmp( gameDir, "baseq3" ) || !Q_stricmp( gameDir, "missionpack" );
}

/*
==============
VM_VRLoadNative

Walk the search paths for the native module; used by vm_* 0 and the
native fallback.
==============
*/
static qboolean VM_VRLoadNative( vm_t *vm ) {
	char filename[MAX_OSPATH];
	void *startSearch = NULL;

	while ( FS_FindVM( &startSearch, filename, sizeof( filename ), vm->name, qfalse ) == VMI_NATIVE ) {
		Com_Printf( "Try loading dll file %s\n", filename );
		vm->dllHandle = Sys_LoadGameDll( filename, &vm->entryPoint, vm->dllSyscall );
		if ( vm->dllHandle ) {
			vm->privateFlag = 0; // allow reading private cvars
			vm->dataAlloc = ~0U;
			vm->dataMask = ~0U;
			vm->dataBase = 0;
			return qtrue;
		}
		Com_Printf( "Failed loading dll, trying next\n" );
	}
	return qfalse;
}

/*
==============
VM_VRModuleLoaded

Hands the fallback notice the pak whose QVM the loaded module replaced ("" for none).
==============
*/
static qboolean VM_VRModuleLoaded( const vm_t *vm, const char *fallbackPak ) {
#ifndef DEDICATED
	if ( vm->index == VM_UI )
		SCR_VRUiFallbackSet( fallbackPak );
	else if ( vm->index == VM_CGAME && fallbackPak[0] )
		SCR_VRFallbackNotice( fallbackPak );
#endif
	return qtrue;
}

/*
==============
VM_VRSelectModule

vm_* 0 loads native first (pure servers excepted). Otherwise the
FS-priority QVM runs when it is VR-aware. A plain QVM carries flatscreen
functions, so the native modules take its place, except that another
mod's plain qagame runs as-is. A pure server requires its own QVM and
another mod's server game requires its own cgame, so those loads fail
and the client returns to the menu.
==============
*/
qboolean VM_VRSelectModule( vm_t *vm, vmInterpret_t *interpret, qboolean qvmOnly, vmHeader_t **header ) {
	char filename[MAX_OSPATH];
	void *startSearch = NULL;
	qboolean triedNative = qfalse;
#ifndef DEDICATED
	qboolean flatWinner = qfalse;
	char flatPakName[MAX_OSPATH];
#endif
	const char *name = vm->name;
	const vmIndex_t index = vm->index;
	const syscall_t systemCall = vm->systemCall;
	const dllSyscall_t dllSyscall = vm->dllSyscall;
	const int privateFlag = vm->privateFlag;

	*header = NULL;
	s_read.buffer = NULL;

	// vm_* 0: honor native (pure servers excepted)
	if ( *interpret == VMI_NATIVE && !qvmOnly ) {
		if ( VM_VRLoadNative( vm ) )
			return VM_VRModuleLoaded( vm, "" );
		// stock fallthrough: no native, run bytecode
		triedNative = qtrue;
	}

	if ( FS_FindVM( &startSearch, filename, sizeof( filename ), name, qtrue ) == VMI_COMPILED ) {
		int major = 0, minor = 0;
		qboolean vrAware;
		const char *pakName = FS_VMSearchPathName( startSearch );
		void *buffer;
		int length = FS_ReadFile( va( "vm/%s.qvm", name ), &buffer );
		if ( buffer )
			major = VM_VRParseMarker( (const byte *)buffer, length, &minor );
		vrAware = major == VR_API_MAJOR && minor <= VR_API_MINOR;
		if ( buffer && !vrAware )
			FS_FreeFile( buffer );
		if ( vrAware ) {
			Com_Printf( "%s: loading VR-aware QVM (VR API %d.%d) from %s\n", name, major, minor, pakName );
			s_read.vm = vm;
			s_read.buffer = buffer;
			s_read.length = length;
			s_read.sentinel = VM_VRAccepts( major, minor, VR_API_MAJOR, VR_API_MINOR );
			*header = VM_LoadQVM( vm, qtrue );
			if ( s_read.buffer )
				FS_FreeFile( s_read.buffer );
			s_read.buffer = NULL;
			if ( *header != NULL ) {
				// a QVM can't run native; execute under the JIT
				if ( *interpret == VMI_NATIVE )
					*interpret = VMI_COMPILED;
				return VM_VRModuleLoaded( vm, "" );
			}
			// VM_LoadQVM wipes the slot on failure; restore identity for the native fallback
			vm->name = name;
			vm->index = index;
			vm->systemCall = systemCall;
			vm->dllSyscall = dllSyscall;
			vm->privateFlag = privateFlag;
		} else if ( major > 0 ) {
			// VR module the engine can't satisfy: drop, so native
			// never silently runs a different game in its place
			Com_Error( ERR_DROP, "%s.qvm (from %s): VR API incompatible: engine %d.%d, mod %d.%d",
				name, pakName, VR_API_MAJOR, VR_API_MINOR, major, minor );
		} else if ( qvmOnly ) {
			Com_Error( ERR_DROP, "VR unavailable - %s QVMs are VR-incompatible. This pure server allows only its own game code.",
				VM_VRFallbackPakName( pakName ) );
		} else if ( index == VM_CGAME && !VM_VRBaseGame( FS_GetCurrentGameDir() ) ) {
			Com_Error( ERR_DROP, "VR unavailable - %s QVMs are VR-incompatible. Trinity's VR modules can't stand in for another mod's game code without breaking gameplay.",
				VM_VRFallbackPakName( pakName ) );
		} else if ( index == VM_GAME && !VM_VRBaseGame( FS_GetCurrentGameDir() ) ) {
			// the server game needs no VR marker, and the mod's cgame expects its own game code
			Com_Printf( "%s.qvm in %s is not VR-aware; running it as this mod's server game\n", name, pakName );
			if ( ( *header = VM_LoadQVM( vm, qtrue ) ) == NULL )
				return qfalse;
			if ( *interpret == VMI_NATIVE )
				*interpret = VMI_COMPILED;
			return VM_VRModuleLoaded( vm, "" );
		} else {
			Com_Printf( "%s.qvm in %s is not VR-aware; loading the native module\n", name, pakName );
#ifndef DEDICATED
			flatWinner = qtrue;
			Q_strncpyz( flatPakName, pakName, sizeof( flatPakName ) );
#endif
		}
	}

	// native fallback, unless the server demands bytecode
	if ( !qvmOnly && !triedNative && VM_VRLoadNative( vm ) ) {
#ifndef DEDICATED
		// announced after the native load succeeds, so the notice names a running fallback
		if ( flatWinner )
			return VM_VRModuleLoaded( vm, VM_VRFallbackPakName( flatPakName ) );
#endif
		return VM_VRModuleLoaded( vm, "" );
	}
	return qfalse;
}

/*
==============
VM_VRLoadQVMFile

Read the pk3-priority QVM and take its VR marker from the bytes loaded.
==============
*/
int VM_VRLoadQVMFile( vm_t *vm, const char *filename, void **buffer ) {
	int length, major, minor;
	vm->vrSentinel = qfalse;
	if ( s_read.buffer && s_read.vm == vm ) {
		*buffer = s_read.buffer;
		vm->vrSentinel = s_read.sentinel;
		s_read.buffer = NULL;
		return s_read.length;
	}
	length = FS_ReadFile( filename, buffer );
	if ( length <= 0 || !buffer || !*buffer ) return length;
	major = VM_VRParseMarker( (const byte *)*buffer, length, &minor );
	vm->vrSentinel = VM_VRAccepts( major, minor, VR_API_MAJOR, VR_API_MINOR );
	return length;
}
