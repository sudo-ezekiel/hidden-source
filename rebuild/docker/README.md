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

## Hidden's exact build: GCC 3.4.5-2

The `.comment` of Hidden's server_i486.so reads `GCC: (GNU) 3.4.6 20060122 (prerelease) (Debian 3.4.5-2)`:
Debian's gcc-3.4 package 3.4.5-2 from January 2006, older than etch's 3.4.6-5. `gcc345/Dockerfile` installs
that build over the image above. Its five packages come from snapshot.debian.org:

| File | Bytes | SHA1 |
|---|---|---|
| cpp-3.4_3.4.5-2_i386.deb | 1,746,064 | bed6dbc5a68775a9ac27b986326a8d41fe924714 |
| gcc-3.4_3.4.5-2_i386.deb | 1,543,744 | 0151a5297aa6fa1dbbacf551c88b83119b007660 |
| g++-3.4_3.4.5-2_i386.deb | 1,989,302 | 93be32709474ff5941fc48b961f7613c2c9d1f32 |
| gcc-3.4-base_3.4.5-2_i386.deb | 163,932 | a3b4e45a9f55b71c1153c3497b0421c1a0c9be72 |
| libstdc++6-dev_3.4.5-2_i386.deb | 1,274,036 | 6b18d7dc7d3a66b242ffd8cfc811255037e4fdd7 |

Each is at `https://snapshot.debian.org/file/<SHA1>`. They were also checked against Debian's own signed index
of the time: sid's `Release` of 2006-01-24 (gpgv: good signature from the Debian Archive Automatic Signing Key
(2006), 2D230C5F, which etch keeps in /usr/share/keyrings/debian-archive-removed-keys.gpg), whose SHA1 for
`main/binary-i386/Packages.gz` matches, and whose MD5 for each package matches.

Put the five files in `gcc345/debs/`, then:

    docker build --platform linux/386 -t hsrebuild-gcc345 rebuild/docker/gcc345

and build with `GCC_IMAGE=hsrebuild-gcc345 sh rebuild/build.sh <work dir>` (client_check.sh takes the same
variable). Measured on main aa60358: every server function compares exactly as with 3.4.6-5 (18366 same /
47 changed, the same similarity and sizes for all 18413), so the remaining differences are not the compiler
version. Only the statically linked runtime and the crt objects' `.comment` differ.
