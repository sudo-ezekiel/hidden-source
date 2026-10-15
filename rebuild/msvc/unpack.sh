#!/bin/sh
# Unpack Visual C++ Toolkit 2003 1.01 and the Windows Server 2003 SP1 Platform SDK into an MSVC 7.1
# tree, without running either installer. See rebuild/msvc/MSVC.md for where the two files come from.
#
#   sh rebuild/msvc/unpack.sh <VC_Toolkit_2003_Setup.exe> <PlatformSDK_Svr2003SP1_rtm.img> <out dir>
#
# Writes <out>/vc71/{bin,include,lib} (cl, link, CRT headers and static CRT libs) and
# <out>/psdk/{Include,Lib} (Win32 headers, x86 import libs). Intermediate files go to <out>/work.
# Needs 7z and python3. Both inputs are checked against their known SHA1 first.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
toolkit=$1
img=$2
out=$3

TOOLKIT_SHA1=50dc6cca1503f08a41d56816f3f1abcaf347b0bd
PSDK_SHA1=ee43b337d9739a4da3417b8eaca36cfafaa41de7

check() {
    got=$(sha1sum "$1" | cut -d' ' -f1)
    if [ "$got" != "$2" ]; then
        echo "unpack.sh: $1 has SHA1 $got, expected $2" >&2
        exit 1
    fi
}
check "$toolkit" "$TOOLKIT_SHA1"
check "$img" "$PSDK_SHA1"

w=$out/work
mkdir -p "$w"

# Toolkit: setup .exe -> InstallShield stream "[0]" -> decoded .msi -> Data1.cab -> files.
7z x -y -o"$w/tk-pe" "$toolkit" > /dev/null
mkdir -p "$w/tk-stream"
python3 -I "$here/is_stream.py" "$w/tk-pe/[0]" "$w/tk-stream"
7z x -y -o"$w/tk-msi" "$w/tk-stream/Microsoft Visual C++ Toolkit 2003.msi" > /dev/null
7z x -y -o"$w/tk-cab" "$w/tk-msi/Data1.cab" > /dev/null
for t in File Component Directory; do
    python3 -I "$here/msi_table.py" "$w/tk-msi" $t > "$w/tk-$t.tsv"
done
mkdir -p "$out/vc71"
python3 -I "$here/msi_layout.py" "$w/tk-File.tsv" "$w/tk-Component.tsv" "$w/tk-Directory.tsv" \
    "$w/tk-cab" "$out/vc71" INSTALLDIR

# Platform SDK: only the x86 MSI and the core build cabinets (headers and x86 libs).
7z x -y -o"$w/psdk-iso" "$img" 'Setup/PSDK-x86.msi' 'Setup/PSDK-SDK_Core_BLD-common.*.cab' \
    'Setup/PSDK-SDK_Core_BLD_X86-common.*.cab' > /dev/null
7z x -y -o"$w/psdk-msi" "$w/psdk-iso/Setup/PSDK-x86.msi" > /dev/null
for c in "$w"/psdk-iso/Setup/*.cab; do
    7z x -y -o"$w/psdk-cab" "$c" > /dev/null
done
for t in File Component Directory; do
    python3 -I "$here/msi_table.py" "$w/psdk-msi" $t > "$w/psdk-$t.tsv"
done
mkdir -p "$out/psdk"
python3 -I "$here/msi_layout.py" "$w/psdk-File.tsv" "$w/psdk-Component.tsv" "$w/psdk-Directory.tsv" \
    "$w/psdk-cab" "$out/psdk" INSTALLLOCATION | grep -v '^skip '

echo "vc71: $(find "$out/vc71" -type f | wc -l) files, psdk: $(find "$out/psdk" -type f | wc -l) files"
