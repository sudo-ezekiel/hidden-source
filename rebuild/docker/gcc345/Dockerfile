# Hidden: Source's exact Linux compiler: Debian gcc-3.4 3.4.5-2 ("GCC 3.4.6 20060122 (prerelease) (Debian 3.4.5-2)",
# the .comment of Hidden's server_i486.so), installed over the etch image's 3.4.6-5. The five .debs come from
# snapshot.debian.org and were checked against sid's Release of 2006-01-24 (gpgv, Debian Archive Automatic Signing
# Key (2006), 2D230C5F) and the MD5 sums in its main/binary-i386/Packages.gz.
FROM --platform=linux/386 hsrebuild-gcc34
COPY debs /tmp/debs
RUN dpkg -i /tmp/debs/*.deb && rm -rf /tmp/debs
WORKDIR /src
