// REBUILD: stand-in for the DirectX SDK's ddraw.h, which Hidden's build machine had and the Platform
// SDK does not. Only what public/nvtc.h and public/imageloader.cpp use: DDSURFACEDESC (DirectX 7
// layout, 0x6c bytes), DDPIXELFORMAT (0x20 bytes) and the DDSD_ and DDPF_ flags they set. The sizes
// and offsets are client.dll's: ImageLoader's DXT branch (24129243) clears 0x1b dwords, sets dwSize
// 0x6c, dwFlags 0x1806, lpSurface at +0x24 and ddpfPixelFormat at +0x48 with dwSize 0x20.
// Used by rebuild/msvc/build_client.sh only; it must come after windows.h, as the real one does.
#ifndef __DDRAW_INCLUDED__
#define __DDRAW_INCLUDED__

typedef struct _DDCOLORKEY
{
	DWORD dwColorSpaceLowValue;
	DWORD dwColorSpaceHighValue;
} DDCOLORKEY;

typedef struct _DDSCAPS
{
	DWORD dwCaps;
} DDSCAPS;

typedef struct _DDPIXELFORMAT
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	union
	{
		DWORD dwRGBBitCount;
		DWORD dwYUVBitCount;
		DWORD dwZBufferBitDepth;
		DWORD dwAlphaBitDepth;
	};
	union
	{
		DWORD dwRBitMask;
		DWORD dwYBitMask;
	};
	union
	{
		DWORD dwGBitMask;
		DWORD dwUBitMask;
	};
	union
	{
		DWORD dwBBitMask;
		DWORD dwVBitMask;
	};
	union
	{
		DWORD dwRGBAlphaBitMask;
		DWORD dwYUVAlphaBitMask;
	};
} DDPIXELFORMAT;

typedef struct _DDSURFACEDESC
{
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwHeight;
	DWORD dwWidth;
	union
	{
		LONG lPitch;
		DWORD dwLinearSize;
	};
	DWORD dwBackBufferCount;
	union
	{
		DWORD dwMipMapCount;
		DWORD dwZBufferBitDepth;
		DWORD dwRefreshRate;
	};
	DWORD dwAlphaBitDepth;
	DWORD dwReserved;
	LPVOID lpSurface;
	DDCOLORKEY ddckCKDestOverlay;
	DDCOLORKEY ddckCKDestBlt;
	DDCOLORKEY ddckCKSrcOverlay;
	DDCOLORKEY ddckCKSrcBlt;
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS ddsCaps;
} DDSURFACEDESC;

#define DDSD_CAPS			0x00000001l
#define DDSD_HEIGHT			0x00000002l
#define DDSD_WIDTH			0x00000004l
#define DDSD_PITCH			0x00000008l
#define DDSD_LPSURFACE		0x00000800l
#define DDSD_PIXELFORMAT	0x00001000l

#define DDPF_ALPHAPIXELS	0x00000001l
#define DDPF_FOURCC			0x00000004l
#define DDPF_RGB			0x00000040l

#endif // __DDRAW_INCLUDED__
