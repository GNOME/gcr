# included in top-level Makefile.am

gck_incdir = $(includedir)/gck-@GCK_MAJOR@/gck

gck_HEADER_FILES = \
	gck/gck.h \
	gck/gck-deprecated.h

gck_inc_HEADERS = \
	$(gck_HEADER_FILES) \
	gck/gck-enum-types.h \
	gck/gck-version.h \
	gck/pkcs11.h \
	gck/pkcs11n.h \
	gck/pkcs11x.h

gck_BUILT_SOURCES = \
	gck/gck-marshal.c gck/gck-marshal.h \
	gck/gck-enum-types.c gck/gck-enum-types.h

BUILT_SOURCES += $(gck_BUILT_SOURCES)

lib_LTLIBRARIES += libgck-@GCK_MAJOR@.la

gck_PUBLIC_FILES = \
	gck/gck.h \
	gck/gck-enum-types.h \
	gck/gck-attributes.c \
	gck/gck-dump.c \
	gck/gck-enumerator.c \
	gck/gck-misc.c \
	gck/gck-module.c \
	gck/gck-modules.c \
	gck/gck-object.c \
	gck/gck-object-cache.c \
	gck/gck-password.c \
	gck/gck-session.c \
	gck/gck-slot.c \
	gck/gck-uri.c \
	gck/gck-version.h

gck_INTERNAL_FILES = \
	gck/gck-call.c \
	gck/gck-debug.c gck/gck-debug.h \
	gck/gck-deprecated.h \
	gck/gck-interaction.c \
	gck/gck-private.h \
	gck/pkcs11.h

libgck_@GCK_MAJOR@_la_SOURCES = \
	$(gck_PUBLIC_FILES) \
	$(gck_INTERNAL_FILES)

nodist_libgck_@GCK_MAJOR@_la_SOURCES = \
	$(gck_BUILT_SOURCES)

libgck_@GCK_MAJOR@_la_CFLAGS = \
	$(P11_KIT_CFLAGS) \
	-DG_LOG_DOMAIN=\"Gck\" \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DP11_KIT_API_SUBJECT_TO_CHANGE \
	-DPKCS11_REGISTRY_DIR=\"$(libdir)/pkcs11\"

libgck_@GCK_MAJOR@_la_LDFLAGS = \
	-version-info $(GCK_LT_RELEASE) \
	-no-undefined \
	-export-symbols-regex '^gck_.*|^SECMEM_.*'

libgck_@GCK_MAJOR@_la_LIBADD = \
	libegg-hex.la \
	libegg-secmem.la \
	$(P11_KIT_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

noinst_LTLIBRARIES += libgck-testable.la
libgck_testable_la_SOURCES = \
	gck/gck-mock.c gck/gck-mock.h \
	gck/gck-test.c gck/gck-test.h

libgck_testable_la_CFLAGS = \
	$(libgck_@GCK_MAJOR@_la_CFLAGS)

libgck_testable_la_LIBADD = \
	$(libgck_@GCK_MAJOR@_la_OBJECTS) \
	libegg-hex.la \
	libegg-secmem.la \
	$(P11_KIT_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)
EXTRA_libgck_testable_la_DEPENDENCIES = $(libgck_@GCK_MAJOR@_la_OBJECTS)

gck/gck-marshal.h: gck/gck-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=_gck_marshal > $@

gck/gck-marshal.c: gck/gck-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"gck/gck-marshal.h\"" > $@ && \
		$(GLIB_GENMARSHAL) $< --body --prefix=_gck_marshal >> $@

gck/gck-enum-types.h: $(ENUM_TEMPLATE_H) $(gck_HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gck/gck-enum-types.c: $(ENUM_TEMPLATE_C) $(gck_HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

EXTRA_DIST += \
	gck/gck.pc.in \
	gck/gck-marshal.list \
	gck/gck-version.h \
	gck/gck.symbols \
	gck/pkcs11-trust-assertions.h \
	gck/pkcs11i.h

CLEANFILES += \
	gck-$(GCK_MAJOR).pc \
	$(gir_DATA) \
	$(typelibs_DATA) \
	$(BUILT_SOURCES) \
	gck-actual.abi \
	gck-expected.abi

# ------------------------------------------------------------------
# INTROSPECTION

if HAVE_INTROSPECTION

INTROSPECTION_GIRS += Gck-@GCK_MAJOR@.gir

Gck-@GCK_MAJOR@.gir: libgck-@GCK_MAJOR@.la

Gck_@GCK_MAJOR@_gir_PACKAGES = gobject-2.0 gio-2.0 p11-kit-1
Gck_@GCK_MAJOR@_gir_EXPORT_PACKAGES = gck-@GCK_MAJOR@
Gck_@GCK_MAJOR@_gir_INCLUDES = GObject-2.0 Gio-2.0
Gck_@GCK_MAJOR@_gir_LIBS = libgck-@GCK_MAJOR@.la
Gck_@GCK_MAJOR@_gir_CFLAGS = -I$(srcdir) -I$(builddir) -DGCK_COMPILATION -DGCK_API_SUBJECT_TO_CHANGE
Gck_@GCK_MAJOR@_gir_FILES = $(gck_PUBLIC_FILES)
Gck_@GCK_MAJOR@_gir_SCANNERFLAGS = --add-include-path=$(srcdir)/gck --c-include "gck/gck.h"

gir_DATA += Gck-@GCK_MAJOR@.gir

if ENABLE_VAPIGEN

gck-@GCK_MAJOR@.vapi: Gck-@GCK_MAJOR@.gir gck/Gck-@GCK_MAJOR@.metadata gck-@GCK_MAJOR@.deps

VAPIGEN_VAPIS += gck-@GCK_MAJOR@.vapi

gck_@GCK_MAJOR@_vapi_DEPS = glib-2.0 gio-2.0
gck_@GCK_MAJOR@_vapi_METADATADIRS = $(srcdir)
gck_@GCK_MAJOR@_vapi_FILES = Gck-@GCK_MAJOR@.gir

gck-$(GCK_MAJOR).deps: Makefile.am
	$(AM_V_GEN) echo $(gck_@GCK_MAJOR@_vapi_DEPS) | tr ' ' '\n' > $@

vapi_DATA += gck/pkcs11.vapi

EXTRA_DIST += \
	gck/Gck-@GCK_MAJOR@.metadata \
	gck/pkcs11.vapi

endif # ENABLE_VAPIGEN

endif

# ----------------------------------------------------------------

pkgconfig_DATA += gck-$(GCK_MAJOR).pc

gck-$(GCK_MAJOR).pc: gck/gck.pc
	$(AM_V_GEN) cp gck/gck.pc gck-$(GCK_MAJOR).pc

gck-expected.abi: gck/gck.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@

gck-actual.abi: $(builddir)/.libs/libgck-@GCK_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end)' | sort > $@

check-gck-symbols: gck-expected.abi gck-actual.abi
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gck-expected.abi gck-actual.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gck.symbols"; exit 1)

gck_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DG_LOG_DOMAIN=\"Gck\" \
	$(P11_KIT_CFLAGS) \
	$(GLIB_CFLAGS)

gck_LIBS =  \
	libgck-testable.la \
	libegg-test.la \
	libegg-hex.la \
	$(GLIB_LIBS) \
	$(P11_KIT_LIBS) \
	$(GIO_LIBS)

gck_TESTS = \
	test-gck-attributes \
	test-gck-module \
	test-gck-slot \
	test-gck-session \
	test-gck-object \
	test-gck-crypto \
	test-gck-uri \
	test-gck-enumerator \
	test-gck-modules

test_gck_attributes_SOURCES = gck/test-gck-attributes.c
test_gck_attributes_CFLAGS = $(gck_CFLAGS)
test_gck_attributes_LDADD = $(gck_LIBS)

test_gck_crypto_SOURCES = gck/test-gck-crypto.c
test_gck_crypto_CFLAGS = $(gck_CFLAGS)
test_gck_crypto_LDADD = $(gck_LIBS)

test_gck_enumerator_SOURCES = \
	gck/test-gck-enumerator.c \
	gck/mock-interaction.c gck/mock-interaction.h
test_gck_enumerator_CFLAGS = $(gck_CFLAGS)
test_gck_enumerator_LDADD = $(gck_LIBS)

test_gck_object_SOURCES = gck/test-gck-object.c
test_gck_object_CFLAGS = $(gck_CFLAGS)
test_gck_object_LDADD = $(gck_LIBS)

test_gck_module_SOURCES = gck/test-gck-module.c
test_gck_module_CFLAGS = $(gck_CFLAGS)
test_gck_module_LDADD = $(gck_LIBS)

test_gck_modules_SOURCES = gck/test-gck-modules.c
test_gck_modules_CFLAGS = $(gck_CFLAGS)
test_gck_modules_LDADD = $(gck_LIBS)

test_gck_session_SOURCES = \
	gck/test-gck-session.c \
	gck/mock-interaction.c gck/mock-interaction.h
test_gck_session_CFLAGS = $(gck_CFLAGS)
test_gck_session_LDADD = $(gck_LIBS)

test_gck_slot_SOURCES = gck/test-gck-slot.c
test_gck_slot_CFLAGS = $(gck_CFLAGS)
test_gck_slot_LDADD = $(gck_LIBS)

test_gck_uri_SOURCES = gck/test-gck-uri.c
test_gck_uri_CFLAGS = $(gck_CFLAGS)
test_gck_uri_LDADD = $(gck_LIBS)

check_PROGRAMS += $(gck_TESTS)
TESTS += $(gck_TESTS)

check_LTLIBRARIES += libmock-test-module.la

libmock_test_module_la_LDFLAGS = \
	-module -avoid-version -shared -rpath $(abs_builddir)

libmock_test_module_la_CFLAGS = \
	-DGCK_API_SUBJECT_TO_CHANGE \
	$(gck_CFLAGS)

libmock_test_module_la_SOURCES = \
	gck/mock-test-module.c

libmock_test_module_la_LIBADD = \
	libgck-testable.la
