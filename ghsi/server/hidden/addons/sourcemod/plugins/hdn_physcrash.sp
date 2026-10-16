#define PLUGIN_VERSION		"1.0"
#include <sourcemod>

public Plugin:myinfo = 
{
	name		= "Hidden Physics Crash Prevention",
	author		= "Hackasaurus Rex",
	description	= "Prevents crashes from dead Hidden picking up objects",
	version		= PLUGIN_VERSION,
	url			= "n/a"
}

public Action:OnPlayerRunCmd(client,&buttons,&impulse,Float:vel[3],Float:angles[3],&weapon)
{
	if (!IsPlayerAlive(client) && buttons & IN_USE)
	{
		buttons &= ~IN_USE;
		return Plugin_Continue;
	} else {
		return Plugin_Continue;
	}
}