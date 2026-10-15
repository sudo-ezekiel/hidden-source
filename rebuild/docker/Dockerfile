# 32-bit Debian etch with GCC 3.4, the compiler family Hidden: Source's Linux server was built with.
# The .debs were checked against etch's signed Release (gpgv, Debian archive keys) and
# the SHA-256 sums in its Packages index before being copied in.
FROM --platform=linux/386 debian/eol:etch
COPY debs /tmp/debs
RUN dpkg -i /tmp/debs/*.deb && rm -rf /tmp/debs
WORKDIR /src
