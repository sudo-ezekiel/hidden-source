// Shim for rebuild/client_check.sh: the few Win32 declarations the client's Windows-only input
// code uses (in_mouse.cpp: SystemParametersInfo; in_joystick.cpp: the winmm joystick API).
// Declarations only, values as in the Platform SDK; nothing here is ever linked.
#ifndef WINDOWS_SHIM_H
#define WINDOWS_SHIM_H

#include "tier0/platform.h" // DWORD, WORD

typedef unsigned int UINT;
typedef int BOOL;
typedef void *PVOID;
typedef void *LPVOID;
typedef UINT MMRESULT;

#define WINAPI

#define SPI_GETMOUSE 0x0003
#define SPI_SETMOUSE 0x0004
extern "C" BOOL WINAPI SystemParametersInfo( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni );

#define JOYERR_NOERROR 0
#define JOY_RETURNX 0x00000001l
#define JOY_RETURNY 0x00000002l
#define JOY_RETURNZ 0x00000004l
#define JOY_RETURNR 0x00000008l
#define JOY_RETURNU 0x00000010l
#define JOY_RETURNV 0x00000020l
#define JOY_RETURNPOV 0x00000040l
#define JOY_RETURNBUTTONS 0x00000080l
#define JOY_RETURNCENTERED 0x00000400l
#define JOY_POVCENTERED (WORD) -1
#define JOY_POVFORWARD 0
#define JOY_POVRIGHT 9000
#define JOY_POVBACKWARD 18000
#define JOY_POVLEFT 27000
#define JOYCAPS_HASPOV 0x0010

typedef struct joyinfoex_tag
{
	DWORD dwSize, dwFlags;
	DWORD dwXpos, dwYpos, dwZpos, dwRpos, dwUpos, dwVpos;
	DWORD dwButtons, dwButtonNumber, dwPOV, dwReserved1, dwReserved2;
} JOYINFOEX, *LPJOYINFOEX;

typedef struct joycaps_tag
{
	WORD wMid, wPid;
	char szPname[32];
	UINT wXmin, wXmax, wYmin, wYmax, wZmin, wZmax, wNumButtons, wPeriodMin, wPeriodMax;
	UINT wRmin, wRmax, wUmin, wUmax, wVmin, wVmax, wCaps, wMaxAxes, wNumAxes, wMaxButtons;
	char szRegKey[32], szOEMVxD[260];
} JOYCAPS, *LPJOYCAPS;

extern "C" UINT WINAPI joyGetNumDevs( void );
extern "C" MMRESULT WINAPI joyGetPosEx( UINT uJoyID, LPJOYINFOEX pji );
extern "C" MMRESULT WINAPI joyGetDevCaps( UINT uJoyID, LPJOYCAPS pjc, UINT cbjc );

#endif // WINDOWS_SHIM_H
