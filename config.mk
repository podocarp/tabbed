# tabbed version
VERSION = 0.0

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# includes and libs
INCS = -I. -I/usr/include
LIBS = -L/usr/lib -lc -lX11

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\"
#CFLAGS = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
#CFLAGS = -std=c99 -pedantic -Wall -Werror -O0 ${INCS} ${CPPFLAGS}
CFLAGS = -g -std=c99 -pedantic -Wall -Werror -O0 ${INCS} ${CPPFLAGS}
#LDFLAGS = -s ${LIBS}
LDFLAGS = ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
