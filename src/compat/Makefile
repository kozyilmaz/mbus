
include ../../Makefile.conf

target.a-y = \
	libmbus-compat.a

target.so-${SHARED_ENABLE} = \
	libmbus-compat.so

libmbus-compat.so_files-y = \
	compat.c

libmbus-compat.a_files-y = \
	${libmbus-compat.so_files-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	compat.h

dist.lib-y = \
	libmbus-compat.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-compat.so

include ../../Makefile.lib
