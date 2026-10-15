// Shim for rebuild/client_check.sh: the MSVC debug heap (c_baseentity.cpp operator new).
#ifndef CRTDBG_SHIM_H
#define CRTDBG_SHIM_H
#include <stdlib.h>
#define _NORMAL_BLOCK 1
#define _malloc_dbg( size, blockUse, file, line ) malloc( size )
#define _free_dbg( p, blockUse ) free( p )
#endif
