# GCC 3.4 build image

Hidden: Source's Linux server was compiled with `GCC 3.4.6 20060122 (prerelease) (Debian 3.4.5-2)`
(from the `.comment` section of `server_i486.so`). This image is 32-bit Debian 4.0 (etch) with
`gcc-3.4` 3.4.6-5, the closest release still in the Debian archive.

The etch signing keys have expired, so apt cannot install from the archive with its checks on.
Instead the packages were checked by hand:

1. `Release` and `Release.gpg` from `archive.debian.org/debian/dists/etch/`, verified with `gpgv`
   against the image's own `/etc/apt/trusted.gpg` (good signatures from the lenny archive key and
   the etch stable release key).
2. `main/binary-i386/Packages.bz2` checked against the SHA-256 in that `Release`.
3. `resolve_debs.py Packages installed.txt gcc-3.4 g++-3.4 make libc6-dev binutils` lists the
   nine packages to fetch with their SHA-256 from `Packages`; each `.deb` was checked against it.

Put the verified `.deb` files in `debs/` next to the Dockerfile, then:

    docker build --platform linux/386 -t hsrebuild-gcc34 .
