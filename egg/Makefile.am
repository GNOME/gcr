include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = \
	libegg.la \
	libegg-asn1x.la \
	libegg-entry-buffer.la \
	libegg-hex.la \
	libegg-test.la

ASN_FILES = \
	pk.asn \
	pkix.asn

ASN_SRCS = $(ASN_FILES:.asn=.asn.h)

BUILT_SOURCES = \
	$(ASN_SRCS)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/build \
	-DWITH_VALGRIND

libegg_la_CFLAGS = \
	$(GLIB_CFLAGS)

libegg_la_SOURCES = \
	egg-armor.c egg-armor.h \
	egg-asn1x.c egg-asn1x.h \
	egg-asn1-defs.c egg-asn1-defs.h \
	egg-buffer.c egg-buffer.h \
	egg-bytes.c egg-bytes.h \
	egg-dh.c egg-dh.h \
	egg-dn.c egg-dn.h \
	egg-error.h \
	egg-decimal.c egg-decimal.h \
	egg-hex.c egg-hex.h \
	egg-hkdf.c egg-hkdf.h \
	egg-libgcrypt.c egg-libgcrypt.h \
	egg-oid.c egg-oid.h \
	egg-padding.c egg-padding.h \
	egg-openssl.c egg-openssl.h \
	egg-secure-memory.c egg-secure-memory.h \
	egg-symkey.c egg-symkey.h \
	egg-testing.c egg-testing.h \
	egg-timegm.c egg-timegm.h \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	$(ASN_FILES)

CLEANFILES = \
	$(BUILT_SOURCES)

# --------------------------------------------------------------------
# COMMON STUFF COMPILED INTO SMALLER COMPONENTS

libegg_asn1x_la_SOURCES = \
	egg-asn1x.c egg-asn1x.h \
	$(BUILT_SOURCES)

libegg_asn1x_la_CFLAGS = \
	$(GLIB_CFLAGS)

libegg_entry_buffer_la_SOURCES = \
	egg-entry-buffer.c egg-entry-buffer.h

libegg_entry_buffer_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS)

libegg_hex_la_SOURCES = \
	egg-hex.c egg-hex.h

libegg_hex_la_CFLAGS = \
	$(GLIB_CFLAGS)

libegg_hex_la_LIBS = \
	$(GLIB_LIBS)

libegg_test_la_SOURCES = \
	egg-testing.c egg-testing.h

libegg_test_la_CFLAGS = \
	$(GLIB_CFLAGS)

libegg_test_la_LIBS = \
	$(GLIB_LIBS)

# -------------------------------------------------------------------

SUBDIRS = . tests

check-memory:
	make -C tests check-memory
