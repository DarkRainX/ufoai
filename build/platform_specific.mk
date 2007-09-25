#########################################################################################################################
# Special libs
#########################################################################################################################

# Defaults
SHARED_EXT=so
SHARED_LDFLAGS=-shared
SHARED_CFLAGS=-fPIC
JPEG_CFLAGS=
CFLAGS+=
LDFLAGS+=

# MinGW32
ifeq ($(TARGET_OS),mingw32)
	CLIENT_LIBS+=-lwsock32 -lwinmm -lkernel32 -luser32 -lgdi32
	SERVER_LIBS+=-lwsock32 -lwinmm -lkernel32 -luser32 -lgdi32
	SHARED_EXT=dll
	SHARED_LDFLAGS=-shared
	SHARED_CFLAGS=-shared
	JPEG_CFLAGS=-DDONT_TYPEDEF_INT32
	CFLAGS+=-DGETTEXT_STATIC
#	GAME_LIBS+=
#	TOOLS_LIBS=
endif

# Linux like
ifeq ($(TARGET_OS),linux-gnu)
	CFLAGS+=-D_GNU_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE
#	-Wunsafe-loop-optimizations
endif

# FreeBSD like
ifeq ($(TARGET_OS),freebsd)
	CFLAGS+=-D_BSD_SOURCE -D_XOPEN_SOURCE
endif

ifeq ($(TARGET_OS),netbsd)
	CFLAGS+=-D_BSD_SOURCE -D_NETBSD_SOURCE
endif

# Darwin
ifeq ($(TARGET_OS),darwin)
	SHARED_EXT=dylib
	SHARED_CFLAGS=-fPIC -fno-common
	SHARED_LDFLAGS=-dynamiclib
	CFLAGS+= -D_BSD_SOURCE -D_XOPEN_SOURCE
	SERVER_LIBS+=
endif

# Solaris
ifeq ($(TARGET_OS),solaris)
	#TODO
	CFLAGS+=
	CLIENT_LIBS+=-lsocket -lnsl
	SERVER_LIBS+=-lsocket -lnsl
	MASTER_LIBS+=-lsocket -lnsl
endif

#########################################################################################################################
# Release flags
#########################################################################################################################

RELEASE_CFLAGS=-ffast-math -funroll-loops -D_FORTIFY_SOURCE=2

CFLAGS+=-DSHARED_EXT=\"$(SHARED_EXT)\"

#ifeq ($(TARGET_CPU),axp)
#	RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations
#endif

#ifeq ($(TARGET_CPU),sparc)
#	RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations
#endif

ifeq ($(TARGET_CPU),powerpc64)
	RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations
	CFLAGS+=-DALIGN_BYTES=1
endif

ifeq ($(TARGET_CPU),powerpc)
	RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations
	CFLAGS+=-DALIGN_BYTES=1
endif

ifeq ($(TARGET_CPU),i386)
	ifeq ($(TARGET_OS),freebsd)
		RELEASE_CFLAGS+=-falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
	else
		RELEASE_CFLAGS+=-O2 -falign-loops=2 -falign-jumps=2 -falign-functions=2 -fno-strict-aliasing
	endif
endif

ifeq ($(TARGET_CPU),x86_64)
	ifeq ($(TARGET_OS),freebsd)
		RELEASE_CFLAGS+=-fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
	else
		RELEASE_CFLAGS+=-O2 -fomit-frame-pointer -fexpensive-optimizations -fno-strict-aliasing
	endif
endif
