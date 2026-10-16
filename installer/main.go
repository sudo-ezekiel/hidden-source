//go:build windows

// Command installer installs Hidden: Source, built from this repository's source, with GHSI's
// Enhanced Edition files, on Windows, or under Wine on Linux for Linux's own Steam (see findSteam).
// installer/build.sh appends the game files to the .exe as a
// tar.gz with two trees: sourcemods/ (the mod, as ghsi.py client installs it) and sdkbase/ (the
// server browser fix's files for Source SDK Base, apart from engine.dll, which is patched in place).
//
//	HiddenSourceInstaller.exe                    install
//	HiddenSourceInstaller.exe -undo-browser-fix  put Source SDK Base's own files back
//	-yes answers every question with the default and does not wait at the end.
package main

import (
	"archive/tar"
	"bufio"
	"bytes"
	"compress/gzip"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"syscall"
	"time"
	"unsafe"
)

var version = "dev" // the source commit, set by build.sh

const payloadMagic = "HSRBPAY1"

// The same engine.dll patch as ghsi/ghsi.py (see ghsi/README.md): Source SDK Base build 124232.
const (
	engineOriginal = "4fc435499f0f18db9f1e3d4af84b463ffd2099cbfa3a1037516dd4aedf7a02b7"
	enginePatched  = "bf871995061f8f83c4e1c511391563cc4e1f109d0540b60e36457f0b6d1bab7e"
)

var enginePatch = []struct {
	off      int
	old, nop []byte
}{
	{0x00AD5B, []byte{0xe8, 0xe0, 0x75, 0x06, 0x00}, bytes.Repeat([]byte{0x90}, 5)}, // the A2C_PRINT message
	{0x1773A9, []byte{0x75, 0x0d}, []byte{0x90, 0x90}},                              // sv_master_legacy_mode
}

const (
	backupDir    = "ghsi-backup" // the same layout as ghsi.py's, so either can undo the other
	newFilesList = "new-files.txt"
)

var (
	yes   = flag.Bool("yes", false, "answer every question with the default and do not wait at the end")
	undo  = flag.Bool("undo-browser-fix", false, "put Source SDK Base's own server browser files back")
	stdin = bufio.NewReader(os.Stdin)
)

func main() {
	flag.Parse()
	fmt.Printf("Hidden: Source, rebuilt from source (%s), with GHSI's Enhanced Edition\n", version)
	fmt.Println("Hidden: Source by Calibre Studios. GHSI by GeeTwentyFive: github.com/GeeTwentyFive/GHSI")
	fmt.Println()
	err := run()
	if err != nil {
		fmt.Println()
		fmt.Println("ERROR:", err)
	}
	if !*yes {
		fmt.Print("\nPress Enter to close.")
		stdin.ReadString('\n')
	}
	if err != nil {
		os.Exit(1)
	}
}

func run() error {
	steam, err := findSteam()
	if err != nil {
		return err
	}
	fmt.Println("Steam:", steam)

	sdk, err := findSDKBase(steam)
	if err != nil {
		return err
	}
	if *undo {
		return undoBrowserFix(sdk)
	}
	if sdk == "" {
		fmt.Println("Source SDK Base 2006 is not installed. Asking Steam to install it...")
		if openSteamURL("steam://install/215") != nil {
			fmt.Println("Steam did not open. Install it from Steam yourself: Library, Tools, Source SDK Base 2006.")
		}
		fmt.Println("Waiting for Steam to finish installing Source SDK Base 2006 (close this window to stop).")
		for sdk == "" {
			time.Sleep(5 * time.Second)
			if sdk, err = findSDKBase(steam); err != nil {
				return err
			}
		}
	}
	fmt.Println("Source SDK Base 2006:", sdk)

	mods := filepath.Join(steam, "steamapps", "sourcemods") // where Linux's Steam always has them
	if !linuxSteam {
		if m, err := regString(`Software\Valve\Steam`, "SourceModInstallPath"); err == nil && m != "" {
			mods = filepath.Clean(filepath.FromSlash(m))
		}
	}
	fmt.Println("Mods folder:", mods)

	for running("hl2.exe") {
		if *yes {
			return errors.New("the game (hl2.exe) is running: close it first")
		}
		ask("The game (hl2.exe) is running. Close it, then press Enter.")
	}

	payload, err := openPayload()
	if err != nil {
		return err
	}
	defer payload.Close()

	backup := filepath.Join(sdk, backupDir)
	_, statErr := os.Stat(backup)
	fresh := os.IsNotExist(statErr) // the browser fix is not applied yet: keep what it replaces
	var newFiles []string
	fmt.Println()
	fmt.Println("Installing...")
	roots := map[string]string{"sourcemods": mods, "sdkbase": sdk}
	err = extract(payload, roots, func(tree, rel, dst string) error {
		if tree != "sdkbase" || !fresh {
			return nil
		}
		if _, err := os.Stat(dst); err == nil {
			return copyFile(dst, filepath.Join(backup, rel))
		}
		newFiles = append(newFiles, filepath.ToSlash(rel))
		return nil
	})
	if err != nil {
		return err
	}
	if fresh {
		if err := patchEngine(sdk, backup); err != nil {
			return err
		}
		sort.Strings(newFiles)
		if err := os.MkdirAll(backup, 0o755); err != nil {
			return err
		}
		list := strings.Join(newFiles, "\n") + "\n"
		if err := os.WriteFile(filepath.Join(backup, newFilesList), []byte(list), 0o644); err != nil {
			return err
		}
	} else if err := patchEngine(sdk, ""); err != nil {
		return err
	}
	fmt.Println("Installed the game in", filepath.Join(mods, "hidden"))
	fmt.Println("Applied the server browser fix to", sdk)
	if linuxSteam {
		fmt.Println("(undo it with: wine HiddenSourceInstaller.exe -undo-browser-fix)")
	} else {
		fmt.Println("(undo it with: HiddenSourceInstaller.exe -undo-browser-fix)")
	}

	fmt.Println()
	if linuxSteam {
		// Linux's Steam cannot be stopped from inside Wine: winebrowser turns steam://exit into
		// steam://exit/, which has not been tried.
		if running("steam") {
			fmt.Println("Restart Steam (Steam menu, Exit, then open it again) and the game is in the library as")
			fmt.Println("\"HIDDEN: SOURCE - Enhanced Edition\".")
			return nil
		}
	} else if running("steam.exe") {
		if *yes || !askYes("Steam has to restart before the game shows in the library. Restart Steam now? [Y/n] ") {
			fmt.Println("Restart Steam yourself; the game is in the library as \"HIDDEN: SOURCE - Enhanced Edition\".")
			return nil
		}
		exec.Command(filepath.Join(steam, "steam.exe"), "-shutdown").Run()
		for i := 0; i < 60 && running("steam.exe"); i++ {
			time.Sleep(time.Second)
		}
		if err := exec.Command(filepath.Join(steam, "steam.exe")).Start(); err != nil {
			return fmt.Errorf("could not start Steam again: %v", err)
		}
	}
	fmt.Println("Done. In Steam, the game is in the library as \"HIDDEN: SOURCE - Enhanced Edition\".")
	return nil
}

// linuxSteam is set when the installer runs under Wine on Linux and installs for Linux's own Steam,
// which runs the game with Proton. That Steam is found in the home folder rather than the registry,
// its files name Unix paths, and its processes are not in Wine's process list.
var linuxSteam bool

// findSteam returns Steam's folder. Under Wine it looks for Linux's Steam first, then for a Windows
// Steam installed in the Wine prefix itself.
func findSteam() (string, error) {
	var home string
	if underWine() {
		home = strings.TrimPrefix(os.Getenv("WINEHOMEDIR"), `\??\`)
		for _, d := range []string{
			`.steam\steam`,
			`.local\share\Steam`,
			`.var\app\com.valvesoftware.Steam\.local\share\Steam`, // Flatpak
			`snap\steam\common\.local\share\Steam`,
			`.steam\debian-installation`,
		} {
			dir := filepath.Join(home, d)
			if st, err := os.Stat(filepath.Join(dir, "steamapps")); home != "" && err == nil && st.IsDir() {
				linuxSteam = true
				return dir, nil
			}
		}
	}
	steam, err := regString(`Software\Valve\Steam`, "SteamPath")
	if err == nil && steam != "" {
		return filepath.Clean(filepath.FromSlash(steam)), nil
	}
	if underWine() {
		return "", fmt.Errorf("Steam was not found in %s (in .steam\\steam, .local\\share\\Steam, or Flatpak's or "+
			"Snap's folder). Install Steam, start it once, then run this installer again", home)
	}
	return "", errors.New("Steam is not installed on this PC. Install it from store.steampowered.com, start it " +
		"once, then run this installer again")
}

// openSteamURL hands a steam:// link to Steam. Under Wine, winebrowser passes it to Linux (xdg-open).
func openSteamURL(url string) error {
	if linuxSteam {
		return exec.Command("winebrowser", url).Run()
	}
	return exec.Command("cmd", "/c", "start", "", url).Run()
}

var (
	kernel32       = syscall.NewLazyDLL("kernel32.dll")
	wineGetDOSName = kernel32.NewProc("wine_get_dos_file_name") // only Wine's kernel32 has it
	getProcessHeap = kernel32.NewProc("GetProcessHeap")
	heapFree       = kernel32.NewProc("HeapFree")
)

func underWine() bool { return wineGetDOSName.Find() == nil }

// dosPath returns the Windows path Wine gives a Unix path (Z:\... with the usual drive setup), or "".
func dosPath(unix string) string {
	b, err := syscall.BytePtrFromString(unix)
	if err != nil || !underWine() {
		return ""
	}
	r, _, _ := wineGetDOSName.Call(uintptr(unsafe.Pointer(b)))
	if r == 0 {
		return ""
	}
	p := *(**uint16)(unsafe.Pointer(&r)) // a UTF-16 string on the process heap
	n := 0
	for *(*uint16)(unsafe.Add(unsafe.Pointer(p), 2*n)) != 0 {
		n++
	}
	s := syscall.UTF16ToString(unsafe.Slice(p, n))
	heap, _, _ := getProcessHeap.Call()
	heapFree.Call(heap, 0, r)
	return s
}

// libraryPath turns a library folder named in libraryfolders.vdf into a path the installer can open.
func libraryPath(p string) string {
	if linuxSteam && strings.HasPrefix(p, "/") {
		return dosPath(p)
	}
	return filepath.Clean(strings.ReplaceAll(p, `\\`, `\`))
}

// findSDKBase returns the Source SDK Base 2006 folder (Steam app 215) once Steam has fully installed
// it, or "" if it is not installed.
func findSDKBase(steam string) (string, error) {
	libs := []string{steam}
	if vdf, err := os.ReadFile(filepath.Join(steam, "steamapps", "libraryfolders.vdf")); err == nil {
		for _, m := range regexp.MustCompile(`"path"\s+"([^"]+)"`).FindAllSubmatch(vdf, -1) {
			if lib := libraryPath(string(m[1])); lib != "" {
				libs = append(libs, lib)
			}
		}
	}
	for _, lib := range libs {
		acf, err := os.ReadFile(filepath.Join(lib, "steamapps", "appmanifest_215.acf"))
		if err != nil {
			continue
		}
		if !regexp.MustCompile(`"StateFlags"\s+"4"`).Match(acf) {
			continue // still downloading or updating
		}
		m := regexp.MustCompile(`"installdir"\s+"([^"]+)"`).FindSubmatch(acf)
		if m == nil {
			continue
		}
		dir := filepath.Join(lib, "steamapps", "common", string(m[1]))
		if _, err := os.Stat(filepath.Join(dir, "bin", "engine.dll")); err == nil {
			return dir, nil
		}
	}
	return "", nil
}

type payloadFile struct {
	*io.SectionReader
	f *os.File
}

func (p payloadFile) Close() error { return p.f.Close() }

// openPayload finds the tar.gz build.sh appended: <exe><payload><offset u64><size u64><magic>.
func openPayload() (payloadFile, error) {
	exe, err := os.Executable()
	if err != nil {
		return payloadFile{}, err
	}
	f, err := os.Open(exe)
	if err != nil {
		return payloadFile{}, err
	}
	st, err := f.Stat()
	if err != nil {
		f.Close()
		return payloadFile{}, err
	}
	trailer := make([]byte, 24)
	if _, err := f.ReadAt(trailer, st.Size()-24); err != nil || string(trailer[16:]) != payloadMagic {
		f.Close()
		return payloadFile{}, errors.New("this installer has no game files in it (it was not made by installer/build.sh)")
	}
	off := int64(binary.LittleEndian.Uint64(trailer[0:]))
	size := int64(binary.LittleEndian.Uint64(trailer[8:]))
	return payloadFile{io.NewSectionReader(f, off, size), f}, nil
}

type progress struct {
	r          io.Reader
	done, size int64
	shown      int64
}

func (p *progress) Read(b []byte) (int, error) {
	n, err := p.r.Read(b)
	p.done += int64(n)
	if pct := p.done * 100 / p.size; pct >= p.shown+5 || (err == io.EOF && pct > p.shown) {
		p.shown = pct
		fmt.Printf("  %3d%%\n", pct)
	}
	return n, err
}

// extract unpacks the payload: each top folder of the tar ("sourcemods", "sdkbase") goes to its root.
// before is called for every file before it is written, with the file's place in its tree and its
// destination, so that what is there can be kept first.
func extract(p payloadFile, roots map[string]string, before func(tree, rel, dst string) error) error {
	gz, err := gzip.NewReader(&progress{r: p, size: p.Size()})
	if err != nil {
		return err
	}
	tr := tar.NewReader(gz)
	for {
		h, err := tr.Next()
		if err == io.EOF {
			return nil
		}
		if err != nil {
			return fmt.Errorf("the installer's game files are damaged: %v", err)
		}
		name := path.Clean(strings.TrimPrefix(h.Name, "./"))
		if name == "." || strings.HasPrefix(name, "/") || strings.Contains(name, `\`) || strings.Contains(name, ":") ||
			name == ".." || strings.HasPrefix(name, "../") {
			return fmt.Errorf("bad path in the installer: %q", h.Name)
		}
		tree, rel, _ := strings.Cut(name, "/")
		root, ok := roots[tree]
		if !ok {
			return fmt.Errorf("unexpected folder %q in the installer", tree)
		}
		if rel == "" {
			continue
		}
		rel = filepath.FromSlash(rel)
		dst := filepath.Join(root, rel)
		switch h.Typeflag {
		case tar.TypeDir:
			if err := os.MkdirAll(dst, 0o755); err != nil {
				return err
			}
		case tar.TypeReg:
			if err := before(tree, rel, dst); err != nil {
				return err
			}
			if err := writeFile(dst, tr, h.ModTime); err != nil {
				return err
			}
		}
	}
}

func writeFile(dst string, r io.Reader, mtime time.Time) error {
	if err := os.MkdirAll(filepath.Dir(dst), 0o755); err != nil {
		return err
	}
	tmp := dst + ".hs-tmp"
	f, err := os.Create(tmp)
	if err != nil {
		return fmt.Errorf("%v (if this is a permissions problem, run the installer as administrator)", err)
	}
	if _, err := io.Copy(f, r); err != nil {
		f.Close()
		os.Remove(tmp)
		return err
	}
	if err := f.Close(); err != nil {
		os.Remove(tmp)
		return err
	}
	os.Chtimes(tmp, mtime, mtime)
	return os.Rename(tmp, dst) // replaces dst (MoveFileEx with MOVEFILE_REPLACE_EXISTING)
}

func copyFile(src, dst string) error {
	f, err := os.Open(src)
	if err != nil {
		return err
	}
	defer f.Close()
	st, err := f.Stat()
	if err != nil {
		return err
	}
	return writeFile(dst, f, st.ModTime())
}

func sha256File(p string) (string, []byte, error) {
	data, err := os.ReadFile(p)
	if err != nil {
		return "", nil, err
	}
	sum := sha256.Sum256(data)
	return hex.EncodeToString(sum[:]), data, nil
}

// patchEngine applies the 7-byte patch to Source SDK Base's engine.dll, after backing it up when
// backup is not "".
func patchEngine(sdk, backup string) error {
	p := filepath.Join(sdk, "bin", "engine.dll")
	have, data, err := sha256File(p)
	if err != nil {
		return err
	}
	switch have {
	case enginePatched:
		return nil
	case engineOriginal:
	default:
		fmt.Println("Note: Source SDK Base's engine.dll is not the build this fix knows (124232); it was left as it is.")
		fmt.Println("      The server list may still work without it.")
		return nil
	}
	if backup != "" {
		if err := copyFile(p, filepath.Join(backup, "bin", "engine.dll")); err != nil {
			return err
		}
	}
	for _, c := range enginePatch {
		if !bytes.Equal(data[c.off:c.off+len(c.old)], c.old) {
			return fmt.Errorf("engine.dll: unexpected bytes at %#x", c.off)
		}
		copy(data[c.off:], c.nop)
	}
	sum := sha256.Sum256(data)
	if hex.EncodeToString(sum[:]) != enginePatched {
		return errors.New("engine.dll: the patched file is not the expected one")
	}
	st, _ := os.Stat(p)
	return writeFile(p, bytes.NewReader(data), st.ModTime())
}

func undoBrowserFix(sdk string) error {
	if sdk == "" {
		return errors.New("Source SDK Base 2006 is not installed")
	}
	backup := filepath.Join(sdk, backupDir)
	list, err := os.ReadFile(filepath.Join(backup, newFilesList))
	if err != nil {
		return fmt.Errorf("the server browser fix is not applied (no %s)", filepath.Join(backup, newFilesList))
	}
	for running("hl2.exe") {
		if *yes {
			return errors.New("the game (hl2.exe) is running: close it first")
		}
		ask("The game (hl2.exe) is running. Close it, then press Enter.")
	}
	var dirs []string
	for _, rel := range strings.Split(string(list), "\n") {
		if rel == "" {
			continue
		}
		os.Remove(filepath.Join(sdk, filepath.FromSlash(rel)))
		dirs = append(dirs, path.Dir(rel))
	}
	err = filepath.Walk(backup, func(p string, info os.FileInfo, err error) error {
		if err != nil || info.IsDir() || filepath.Base(p) == newFilesList {
			return err
		}
		rel, _ := filepath.Rel(backup, p)
		return copyFile(p, filepath.Join(sdk, rel))
	})
	if err != nil {
		return err
	}
	// the folders the fix created, deepest first
	sort.Slice(dirs, func(i, j int) bool { return len(dirs[i]) > len(dirs[j]) })
	for _, d := range dirs {
		for d != "." && d != "" {
			if os.Remove(filepath.Join(sdk, filepath.FromSlash(d))) != nil {
				break // not empty
			}
			d = path.Dir(d)
		}
	}
	if err := os.RemoveAll(backup); err != nil {
		return err
	}
	fmt.Println("Put Source SDK Base's own server browser files back in", sdk)
	return nil
}

// running says whether a program is running. For Linux's Steam it reads Linux's process list, where
// both Steam and the game under Proton (another Wine) are: Wine's tasklist sees neither.
func running(image string) bool {
	if linuxSteam {
		proc := dosPath("/proc")
		ents, _ := os.ReadDir(proc)
		for _, e := range ents {
			if c := e.Name()[0]; c < '0' || c > '9' {
				continue
			}
			comm, err := os.ReadFile(filepath.Join(proc, e.Name(), "comm"))
			if err == nil && strings.EqualFold(strings.TrimSpace(string(comm)), image) {
				return true
			}
		}
		return false
	}
	out, err := exec.Command("tasklist", "/FI", "IMAGENAME eq "+image, "/NH").Output()
	return err == nil && bytes.Contains(bytes.ToLower(out), []byte(strings.ToLower(image)))
}

func ask(prompt string) string {
	fmt.Print(prompt)
	s, _ := stdin.ReadString('\n')
	return strings.TrimSpace(s)
}

func askYes(prompt string) bool {
	a := strings.ToLower(ask(prompt))
	return a == "" || a == "y" || a == "yes"
}

// regString reads a REG_SZ value under HKEY_CURRENT_USER.
func regString(key, value string) (string, error) {
	const hkcu = 0x80000001
	const rrfRtRegSz = 0x00000002
	proc := syscall.NewLazyDLL("advapi32.dll").NewProc("RegGetValueW")
	k, _ := syscall.UTF16PtrFromString(key)
	v, _ := syscall.UTF16PtrFromString(value)
	var size uint32
	r, _, _ := proc.Call(hkcu, uintptr(unsafe.Pointer(k)), uintptr(unsafe.Pointer(v)), rrfRtRegSz, 0, 0,
		uintptr(unsafe.Pointer(&size)))
	if r != 0 {
		return "", syscall.Errno(r)
	}
	buf := make([]uint16, size/2+1)
	r, _, _ = proc.Call(hkcu, uintptr(unsafe.Pointer(k)), uintptr(unsafe.Pointer(v)), rrfRtRegSz, 0,
		uintptr(unsafe.Pointer(&buf[0])), uintptr(unsafe.Pointer(&size)))
	if r != 0 {
		return "", syscall.Errno(r)
	}
	return syscall.UTF16ToString(buf), nil
}
