// Forced include for rebuild/client_check.sh, the GCC 3.4 compile check of the client.dll sources.
//
// MSVC accepts a forward-declared enum ("enum ImageFormat;") before the enum is defined; GCC does
// not. The client headers rely on that in several places, so the headers that define those enums
// are pulled in first here. Each has an include guard, so the later real includes are no-ops.
#ifndef CLIENT_SHIM_H
#define CLIENT_SHIM_H

// stdlib.h declares random(), which clashes with the engine's "IUniformRandomStream *random".
// The server solves it the same way in dlls/enginecallback.h (#define random random_valve, _LINUX).
#include <stdlib.h>
#define random random_valve

// MSVC built-ins and CRT headers the Windows code takes for granted.
#include <wchar.h>
#include <wctype.h>
#include <limits.h>
#define _snwprintf swprintf
#define raw_name name   // MSVC's type_info::raw_name(), used in Msg() text only
#ifndef __cdecl
#define __cdecl
#endif
#ifndef _cdecl
#define _cdecl
#endif

#include "imageloader.h"                    // ImageFormat (cdll_util.h, itexture.h, vtf.h)
#include "materialsystem/imaterialvar.h"    // MaterialVarType_t (functionproxy.h, proxyentity.h)
#include "materialsystem/imaterialsystem.h" // MaterialRenderTargetDepth_t, RenderTargetSizeMode_t
#include "shake.h"                          // ShakeCommand_t (cdll_util.h, IEffects.h)
#include "soundflags.h"                     // soundlevel_t (imovehelper.h)
#include "const.h"                          // SolidType_t (ICollideable.h)
#include "vgui/MouseCode.h"                 // MouseCode (vgui_controls)
#include "vgui/KeyCode.h"                   // KeyCode (vgui_controls)

// GCC 3.4 checks non-dependent names in templates where the template is defined; MSVC 7.1 waits
// for instantiation. interpolatedvar.h uses gpGlobals->curtime before cdll_client_int.h's
// CGlobalVarsBase is complete.
#include "globalvars_base.h"

#endif // CLIENT_SHIM_H
