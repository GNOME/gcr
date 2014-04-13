# included in top-level Makefile.am

noinst_LTLIBRARIES += \
	libegg.la \
	libegg-asn1x.la \
	libegg-hex.la \
	libegg-secmem.la \
	libegg-test.la

libegg_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(LIBGCRYPT_CFLAGS)

libegg_la_LIBADD = \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS)

libegg_la_SOURCES = \
	egg/egg-armor.c egg/egg-armor.h \
	egg/egg-asn1x.c egg/egg-asn1x.h \
	egg/egg-asn1-defs.c egg/egg-asn1-defs.h \
	egg/egg-buffer.c egg/egg-buffer.h \
	egg/egg-dh.c egg/egg-dh.h \
	egg/egg-dn.c egg/egg-dn.h \
	egg/egg-error.h \
	egg/egg-decimal.c egg/egg-decimal.h \
	egg/egg-hex.c egg/egg-hex.h \
	egg/egg-hkdf.c egg/egg-hkdf.h \
	egg/egg-libgcrypt.c egg/egg-libgcrypt.h \
	egg/egg-oid.c egg/egg-oid.h \
	egg/egg-padding.c egg/egg-padding.h \
	egg/egg-openssl.c egg/egg-openssl.h \
	egg/egg-secure-memory.c egg/egg-secure-memory.h \
	egg/egg-symkey.c egg/egg-symkey.h \
	egg/egg-testing.c egg/egg-testing.h \
	egg/egg-timegm.c egg/egg-timegm.h \
	egg/pk.asn.h egg/pkix.asn.h \
	$(NULL)

# --------------------------------------------------------------------
# COMMON STUFF COMPILED INTO SMALLER COMPONENTS

libegg_asn1x_la_SOURCES = \
	egg/egg-asn1x.c egg/egg-asn1x.h

libegg_asn1x_la_CFLAGS = \
	$(GLIB_CFLAGS)

libegg_hex_la_SOURCES = \
	egg/egg-hex.c egg/egg-hex.h

libegg_hex_la_CFLAGS = \
	$(GLIB_CFLAGS)

libegg_hex_la_LIBADD = \
	$(GLIB_LIBS)

libegg_secmem_la_SOURCES = \
	egg/egg-secure-memory.c egg/egg-secure-memory.h

libegg_test_la_SOURCES = \
	egg/egg-testing.c egg/egg-testing.h

libegg_test_la_CFLAGS = \
	$(GLIB_CFLAGS)

libegg_test_la_LIBADD = \
	$(GLIB_LIBS)

# -------------------------------------------------------------------

egg_LDADD =  \
	libegg.la \
	$(LIBGCRYPT_LIBS) \
	$(GLIB_LIBS)

egg_TESTS = \
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

test_armor_SOURCES = egg/test-armor.c
test_armor_LDADD = $(egg_LDADD)

test_asn1_SOURCES = egg/test-asn1.c egg/test.asn.h
test_asn1_LDADD = $(egg_LDADD)

test_asn1x_SOURCES = egg/test-asn1x.c
test_asn1x_LDADD = $(egg_LDADD)

test_decimal_SOURCES = egg/test-decimal.c
test_decimal_LDADD = $(egg_LDADD)

test_dh_SOURCES = egg/test-dh.c
test_dh_LDADD = $(egg_LDADD)

test_dn_SOURCES = egg/test-dn.c
test_dn_LDADD = $(egg_LDADD)

test_hex_SOURCES = egg/test-hex.c
test_hex_LDADD = $(egg_LDADD)

test_hkdf_SOURCES = egg/test-hkdf.c
test_hkdf_LDADD = $(egg_LDADD)

test_oid_SOURCES = egg/test-oid.c
test_oid_LDADD = $(egg_LDADD)

test_openssl_SOURCES = egg/test-openssl.c
test_openssl_LDADD = $(egg_LDADD)

test_padding_SOURCES = egg/test-padding.c
test_padding_LDADD = $(egg_LDADD)

test_secmem_SOURCES = egg/test-secmem.c
test_secmem_LDADD = $(egg_LDADD)

test_symkey_SOURCES = egg/test-symkey.c
test_symkey_LDADD = $(egg_LDADD)

check_PROGRAMS += $(egg_TESTS)
TESTS += $(egg_TESTS)

ASN_SRCS = \
	egg/pk.asn \
	egg/pkix.asn \
	egg/test.asn \
	$(NULL)

asn:
	for asn in $(ASN_SRCS); do \
		asn1Parser -o $(srcdir)/$$asn.h $(srcdir)/$$asn; \
		sed -i 's|#include.*|/* \0 */|' $(srcdir)/$$asn.h; \
	done

EXTRA_DIST += \
	egg/fixtures \
	$(ASN_SRCS) \
	$(NULL)
