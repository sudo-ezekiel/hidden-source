//go:build windows

// Command installer installs Hidden: Source, built from this repository's source, with GHSI's
// Enhanced Edition files, on Windows. installer/build.sh appends the game files to the .exe as a
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
	steam, err := regString(`Software\Valve\Steam`, "SteamPath")
	if err != nil {
		return fmt.Errorf("Steam is not installed (no SteamPath in the registry): %v", err)
	}
	steam = filepath.Clean(filepath.FromSlash(steam))
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
		if exec.Command("cmd", "/c", "start", "", "steam://install/215").Run() != nil {
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

	mods, err := regString(`Software\Valve\Steam`, "SourceModInstallPath")
	if err != nil || mods == "" {
		mods = filepath.Join(steam, "steamapps", "sourcemods")
	}
	mods = filepath.Clean(filepath.FromSlash(mods))
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
	fmt.Println("(undo it with: HiddenSourceInstaller.exe -undo-browser-fix)")

	fmt.Println()
	if running("steam.exe") {
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

// findSDKBase returns the Source SDK Base 2006 folder (Steam app 215) once Steam has fully installed
// it, or "" if it is not installed.
func findSDKBase(steam string) (string, error) {
	libs := []string{steam}
	if vdf, err := os.ReadFile(filepath.Join(steam, "steamapps", "libraryfolders.vdf")); err == nil {
		for _, m := range regexp.MustCompile(`"path"\s+"([^"]+)"`).FindAllSubmatch(vdf, -1) {
			libs = append(libs, filepath.Clean(strings.ReplaceAll(string(m[1]), `\\`, `\`)))
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

func running(image string) bool {
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
