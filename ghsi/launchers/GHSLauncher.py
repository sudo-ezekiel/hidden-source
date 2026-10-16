import psutil
import miniupnpc
import winreg
import subprocess
import win32gui
import time


TARGET_PROCESS_NAME = "hl2.exe"
HIDDEN_SOURCE_WINDOW_NAME = "HIDDEN: SOURCE - Enhanced Edition"


for p in psutil.process_iter(['name']):
	if p.name() == TARGET_PROCESS_NAME:
		p.kill()


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


steam_path = winreg.QueryValueEx(
	winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Software\\Valve\\Steam"),
	"SteamPath"
)[0] + "\\steam.exe"

hidden_source_path = winreg.QueryValueEx(
	winreg.OpenKey(winreg.HKEY_CURRENT_USER, "Software\\Valve\\Steam"),
	"SourceModInstallPath"
)[0] + "\\hidden"

subprocess.Popen([steam_path, "-applaunch", "215", "-game", hidden_source_path, "-console"])


hidden_hwnd = 0
while not hidden_hwnd:
	hidden_hwnd = win32gui.FindWindow(None, HIDDEN_SOURCE_WINDOW_NAME)
	time.sleep(0.1)

win32gui.SetWindowLong(hidden_hwnd, -16,
	win32gui.GetWindowLong(hidden_hwnd, -16) & ~(
		0x00800000 |	# WS_BORDER
		0x00400000 |	# WS_DLGFRAME
		0x00010000 |	# WS_MAXIMIZEBOX
		0x00020000 |	# WS_MINIMIZEBOX
		0x00080000 |	# WS_SYSMENU
		0x00040000	# WS_THICKFRAME
	)
)
win32gui.SetWindowLong(hidden_hwnd, -20,
	win32gui.GetWindowLong(hidden_hwnd, -20) & ~(
		0x00000001 |	# WS_EX_DLGMODALFRAME
		0x02000000 |	# WS_EX_COMPOSITED
		0x00000100 |	# WS_EX_WINDOWEDGE
		0x00000200 |	# WS_EX_CLIENTEDGE
		0x00080000 |	# WS_EX_LAYERED
		0x00020000 |	# WS_EX_STATICEDGE
		0x00000080 |	# WS_EX_TOOLWINDOW
		0x00040000	# WS_EX_APPWINDOW
	)
)
win32gui.SetWindowPos(hidden_hwnd, None, 0, 0, 0, 0,
	(
		0x0020 |	# SWP_FRAMECHANGED
		0x0040 |	# SWP_SHOWWINDOW
		0x0001 |	# SWP_NOSIZE
		0x0002 |	# SWP_NOMOVE
                0x0400		# SWP_NOSENDCHANGING
	)
)
win32gui.ShowWindow(hidden_hwnd, 3)
