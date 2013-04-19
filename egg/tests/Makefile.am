include $(top_srcdir)/Makefile.decl

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DSRCDIR="\"@abs_srcdir@\"" \
	$(GLIB_CFLAGS) \
	$(LIBGCRYPT_CFLAGS)

LDADD =  \
	$(top_builddir)/egg/libegg.la \
	$(LIBGCRYPT_LIBS) \
	$(GTHREAD_LIBS) \
	$(GLIB_LIBS)

TEST_PROGS = \
	test-asn1 \
	test-asn1x \
	test-dn \
	test-decimal \
	test-hex \
	test-hkdf \
	test-oid \
	test-secmem \
	test-padding \
	test-symkey \
	test-armor \
	test-openssl \
	test-dh

test_asn1_SOURCES = \
	test-asn1.c \
	test.asn.h \
	$(NULL)

check_PROGRAMS = $(TEST_PROGS)

test: $(TEST_PROGS)
	gtester --verbose -m $(TEST_MODE) --g-fatal-warnings $(TEST_PROGS)

check-local: test

check-memory: perform-memcheck

all-local: $(check_PROGRAMS)

EXTRA_DIST = \
	test.asn \
	files \
	$(NULL)

asn:
	asn1Parser -o test.asn.h test.asn
	sed -i 's|#include.*|/* \0 */|' test.asn.h
