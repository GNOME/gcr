include $(top_srcdir)/Makefile.decl

SUBDIRS = . tests

incdir = $(includedir)/gck-@GCK_MAJOR@/gck

HEADER_FILES = \
	gck.h \
	gck-deprecated.h

inc_HEADERS = \
	$(HEADER_FILES) \
	gck-enum-types.h \
	pkcs11.h \
	pkcs11n.h \
	pkcs11x.h

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir)/gcr \
	-I$(top_srcdir) \
	$(P11_KIT_CFLAGS) \
	$(GTHREAD_CFLAGS) \
	$(GLIB_CFLAGS) \
	-DG_LOG_DOMAIN=\"Gck\" \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DP11_KIT_API_SUBJECT_TO_CHANGE \
	-DPKCS11_REGISTRY_DIR=\"$(libdir)/pkcs11\"

BUILT_SOURCES = \
	gck-marshal.c gck-marshal.h \
	gck-enum-types.c gck-enum-types.h

lib_LTLIBRARIES = libgck-@GCK_MAJOR@.la

PUBLIC_FILES = \
	gck.h \
	gck-enum-types.h \
	gck-attributes.c \
	gck-dump.c \
	gck-enumerator.c \
	gck-misc.c \
	gck-module.c \
	gck-modules.c \
	gck-object.c \
	gck-object-cache.c \
	gck-password.c \
	gck-session.c \
	gck-slot.c \
	gck-uri.c

INTERNAL_FILES = \
	gck-call.c \
	gck-debug.c gck-debug.h \
	gck-deprecated.h \
	gck-interaction.c \
	gck-private.h \
	pkcs11.h

libgck_@GCK_MAJOR@_la_SOURCES = \
	$(PUBLIC_FILES) \
	$(INTERNAL_FILES)

nodist_libgck_@GCK_MAJOR@_la_SOURCES = \
	$(BUILT_SOURCES)

libgck_@GCK_MAJOR@_la_LDFLAGS = \
	-version-info $(GCK_LT_RELEASE) \
	-no-undefined \
	-export-symbols-regex '^gck_.*|^SECMEM_.*'

libgck_@GCK_MAJOR@_la_LIBADD = \
	$(top_builddir)/egg/libegg-hex.la \
	$(top_builddir)/egg/libegg-secmem.la \
	$(P11_KIT_LIBS) \
	$(GTHREAD_LIBS) \
	$(GIO_LIBS) \
	$(GLIB_LIBS)

noinst_LTLIBRARIES = libgck-testable.la
libgck_testable_la_SOURCES = \
	gck-mock.c \
	gck-mock.h \
	gck-test.c \
	gck-test.h
libgck_testable_la_LIBADD = $(libgck_@GCK_MAJOR@_la_OBJECTS) \
	$(libgck_@GCK_MAJOR@_la_LIBADD)
libgck_testable_la_DEPENDENCIES = $(libgck_@GCK_MAJOR@_la_OBJECTS)

gck-marshal.h: gck-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=_gck_marshal > $@

gck-marshal.c: gck-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) echo "#include \"gck-marshal.h\"" > $@ && \
		$(GLIB_GENMARSHAL) $< --body --prefix=_gck_marshal >> $@

gck-enum-types.h: $(ENUM_TEMPLATE_H) $(HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gck-enum-types.c: $(ENUM_TEMPLATE_C) $(HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

EXTRA_DIST = \
	gck.pc.in \
	gck-marshal.list \
	gck.symbols \
	pkcs11-trust-assertions.h \
	pkcs11i.h

CLEANFILES = \
	gck-$(GCK_MAJOR).pc \
	$(gir_DATA) \
	$(typelibs_DATA) \
	$(BUILT_SOURCES) \
	gck-actual.abi \
	gck-expected.abi

# ------------------------------------------------------------------
# INTROSPECTION

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS = Gck-@GCK_MAJOR@.gir
INTROSPECTION_SCANNER_ARGS = $(INTROSPECTION_FLAGS) --warn-all --add-include-path=$(srcdir) --add-include-path=.
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir) --includedir=.

Gck-@GCK_MAJOR@.gir: libgck-@GCK_MAJOR@.la

Gck_@GCK_MAJOR@_gir_PACKAGES = gobject-2.0 gio-2.0 p11-kit-1
Gck_@GCK_MAJOR@_gir_EXPORT_PACKAGES = gck-@GCK_MAJOR@
Gck_@GCK_MAJOR@_gir_INCLUDES = GObject-2.0 Gio-2.0
Gck_@GCK_MAJOR@_gir_LIBS = libgck-@GCK_MAJOR@.la
Gck_@GCK_MAJOR@_gir_CFLAGS = -I$(top_srcdir) -I$(top_builddir) -DGCK_COMPILATION -DGCK_API_SUBJECT_TO_CHANGE
Gck_@GCK_MAJOR@_gir_FILES = $(PUBLIC_FILES)
Gck_@GCK_MAJOR@_gir_SCANNERFLAGS = --c-include "gck.h"

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

if ENABLE_VAPIGEN
include $(VAPIGEN_MAKEFILE)

gck-@GCK_MAJOR@.vapi: Gck-@GCK_MAJOR@.gir Gck-@GCK_MAJOR@.metadata gck-@GCK_MAJOR@.deps

VAPIGEN_VAPIS = gck-@GCK_MAJOR@.vapi

gck_@GCK_MAJOR@_vapi_DEPS = glib-2.0 gio-2.0
gck_@GCK_MAJOR@_vapi_METADATADIRS = $(srcdir)
gck_@GCK_MAJOR@_vapi_FILES = Gck-@GCK_MAJOR@.gir

VAPI_DEPS = $(VAPIGEN_VAPIS:.vapi=.deps)

gck-$(GCK_MAJOR).deps: Makefile.am
	$(AM_V_GEN) echo $(gck_@GCK_MAJOR@_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/vala/vapi
vapi_DATA = \
	pkcs11.vapi \
	$(VAPIGEN_VAPIS) \
	$(VAPI_DEPS)

EXTRA_DIST += \
	Gck-@GCK_MAJOR@.metadata \
	pkcs11.vapi

CLEANFILES += \
	$(VAPIGEN_VAPIS) \
	$(VAPI_DEPS)

endif # ENABLE_VAPIGEN

endif

# ----------------------------------------------------------------

pkgconfig_DATA = gck-$(GCK_MAJOR).pc

gck-$(GCK_MAJOR).pc: gck.pc
	$(AM_V_GEN) cp gck.pc gck-$(GCK_MAJOR).pc

gck-expected.abi: gck.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@

gck-actual.abi: $(builddir)/.libs/libgck-@GCK_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end)' | sort > $@

check-symbols: gck-expected.abi gck-actual.abi
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gck-expected.abi gck-actual.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gck.symbols"; exit 1)

check-local: check-symbols

check-memory:
	make -C tests check-memory

