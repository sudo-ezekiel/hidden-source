import miniupnpc
import os
import sys


try:
        upnp = miniupnpc.UPnP()
        if not upnp.discover():
                raise RuntimeError("No UPnP devices found")
        upnp.selectigd()
        upnp.addportmapping(
		27015,
		"UDP",
		upnp.lanaddr,
		27015,
		"Source",
		""
	)
        upnp.addportmapping(
		27015,
		"TCP",
		upnp.lanaddr,
		27015,
		"Source",
		""
	)
except Exception as e:
	print("UPnP ERROR: " + str(e))


os.system("start .\\ServerFiles\\srcds.exe -game hidden -tickrate 128 " + " ".join(sys.argv))