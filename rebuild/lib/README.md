# Engine libraries for linking

`tier0_i486.so` and `vstdlib_i486.so` are Valve's Linux engine libraries, needed only to resolve
symbols when linking `server_i486.so`. The 2004 SDK expected them from a dedicated server install
(`GAME_DIR/bin`). These copies come from github.com/alliedmodders/hl2sdk, branch episode1, commit
2f37703 ("Added .so files needed for linking plugins").
