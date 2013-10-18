include $(top_srcdir)/Makefile.decl

SUBDIRS = . tests

incdir = $(includedir)/gcr-@GCR_MAJOR@/gcr

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(P11_KIT_CFLAGS) \
	-DG_LOG_DOMAIN=\"Gcr\" \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DLOCALEDIR=\""$(datadir)/locale"\"

check-local: check-symbols

check-memory:
	make -C tests check-memory

HEADER_FILES = \
	gcr-base.h \
	gcr-certificate.h \
	gcr-certificate-chain.h \
	gcr-certificate-request.h \
	gcr-collection.h \
	gcr-column.h \
	gcr-comparable.h \
	gcr-deprecated-base.h \
	gcr-fingerprint.h \
	gcr-filter-collection.h \
	gcr-icons.h \
	gcr-importer.h \
	gcr-import-interaction.h \
	gcr-library.h \
	gcr-mock-prompter.h \
	gcr-parser.h \
	gcr-pkcs11-certificate.h \
	gcr-prompt.h \
	gcr-secret-exchange.h \
	gcr-secure-memory.h \
	gcr-simple-certificate.h \
	gcr-simple-collection.h \
	gcr-system-prompt.h \
	gcr-system-prompter.h \
	gcr-trust.h \
	gcr-types.h \
	gcr-union-collection.h \
	gcr-unlock-options.h

inc_HEADERS = \
	$(HEADER_FILES) \
	gcr-enum-types-base.h

if WITH_GTK
inc_HEADERS += gcr.h
endif

lib_LTLIBRARIES = \
	libgcr-base-@GCR_MAJOR@.la

BUILT_SOURCES = \
	gcr-marshal.c gcr-marshal.h \
	gcr-enum-types-base.c gcr-enum-types-base.h \
	gcr-oids.c gcr-oids.h \
	gcr-dbus-generated.c gcr-dbus-generated.h

PUBLIC_FILES = \
	gcr-base.h \
	gcr-certificate.c gcr-certificate.h \
	gcr-certificate-chain.c gcr-certificate-chain.h \
	gcr-certificate-request.c gcr-certificate-request.h \
	gcr-collection.c gcr-collection.h \
	gcr-column.h \
	gcr-comparable.c gcr-comparable.h \
	gcr-filter-collection.c gcr-filter-collection.h \
	gcr-fingerprint.c gcr-fingerprint.h \
	gcr-icons.c gcr-icons.h \
	gcr-importer.c gcr-importer.h \
	gcr-import-interaction.c gcr-import-interaction.h \
	gcr-library.c gcr-library.h \
	gcr-mock-prompter.c gcr-mock-prompter.h \
	gcr-parser.c gcr-parser.h \
	gcr-pkcs11-certificate.c gcr-pkcs11-certificate.h \
	gcr-prompt.c gcr-prompt.h \
	gcr-secret-exchange.c gcr-secret-exchange.h \
	gcr-secure-memory.c gcr-secure-memory.h \
	gcr-simple-certificate.c gcr-simple-certificate.h \
	gcr-simple-collection.c gcr-simple-collection.h \
	gcr-system-prompt.c gcr-system-prompt.h \
	gcr-system-prompter.c gcr-system-prompter.h \
	gcr-types.h \
	gcr-union-collection.c gcr-union-collection.h \
	gcr-unlock-options.h \
	$(NULL)

PRIVATE_FILES = \
	gcr-callback-output-stream.c gcr-callback-output-stream.h \
	gcr-certificate-extensions.c gcr-certificate-extensions.h \
	gcr-column.c \
	gcr-dbus-constants.h \
	gcr-debug.c gcr-debug.h \
	gcr-deprecated-base.h \
	gcr-gnupg-collection.c gcr-gnupg-collection.h \
	gcr-gnupg-importer.c gcr-gnupg-importer.h \
	gcr-gnupg-key.c gcr-gnupg-key.h \
	gcr-gnupg-process.c gcr-gnupg-process.h \
	gcr-gnupg-records.c gcr-gnupg-records.h \
	gcr-gnupg-util.c gcr-gnupg-util.h \
	gcr-internal.h \
	gcr-key-mechanisms.h gcr-key-mechanisms.c \
	gcr-memory-icon.c gcr-memory-icon.h \
	gcr-openpgp.c gcr-openpgp.h \
	gcr-openssh.c gcr-openssh.h \
	gcr-pkcs11-importer.c gcr-pkcs11-importer.h \
	gcr-record.c gcr-record.h \
	gcr-single-collection.c gcr-single-collection.h \
	gcr-subject-public-key.c gcr-subject-public-key.h \
	gcr-trust.c gcr-trust.h \
	gcr-util.c gcr-util.h \
	$(NULL)

libgcr_base_@GCR_MAJOR@_la_SOURCES = \
	$(PUBLIC_FILES) \
	$(PRIVATE_FILES) \
	$(NULL)

nodist_libgcr_base_@GCR_MAJOR@_la_SOURCES = \
	$(BUILT_SOURCES)

libgcr_base_@GCR_MAJOR@_la_CFLAGS = \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DP11_KIT_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION

libgcr_base_@GCR_MAJOR@_la_LDFLAGS = \
	-version-info $(GCR_LT_RELEASE) \
	-no-undefined \
	-export-symbols-regex '^_gcr_.*|^gcr_.*|^SECMEM_.*'

libgcr_base_@GCR_MAJOR@_la_LIBADD = \
	$(top_builddir)/egg/libegg.la \
	$(top_builddir)/gck/libgck-@GCK_MAJOR@.la \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS)

gcr-marshal.h: gcr-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=_gcr_marshal > $@

gcr-marshal.c: gcr-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) ( echo "#include \"gcr-marshal.h\"" > $@ && \
		$(GLIB_GENMARSHAL) $< --body --prefix=_gcr_marshal >> $@ )

gcr-enum-types-base.h: $(ENUM_TEMPLATE_H) $(HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@
gcr-enum-types-base.c: $(ENUM_TEMPLATE_C) $(HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

# $(AM_V_GEN) 

gcr-oids.c: gcr-oids.list gcr-mkoids
	$(AM_V_GEN) sh $(srcdir)/gcr-mkoids -p GCR -c gcr-oids.c -h gcr-oids.h $(srcdir)/gcr-oids.list

gcr-oids.h: gcr-oids.c

DBUS_XML_DEFINITIONS = \
	org.gnome.keyring.Prompter.xml

gcr-dbus-generated.c: $(DBUS_XML_DEFINITIONS)
	$(AM_V_GEN) gdbus-codegen --interface-prefix org.gnome.keyring.internal. \
		--generate-c-code gcr-dbus-generated --c-namespace GcrDBus $^
	$(AM_V_GEN) sed -i -e 's/gcr_dbus/_gcr_dbus/g' gcr-dbus-generated.[ch]

gcr-dbus-generated.h: gcr-dbus-generated.c

pkgconfig_DATA = \
	gcr-base-$(GCR_MAJOR).pc

gcr-base-$(GCR_MAJOR).pc: gcr-base.pc
	$(AM_V_GEN) cp gcr-base.pc gcr-base-$(GCR_MAJOR).pc

service_in_files = \
	org.gnome.keyring.SystemPrompter.service.in \
	org.gnome.keyring.PrivatePrompter.service.in
servicedir       = $(DBUS_SERVICES_DIR)
service_DATA     = $(service_in_files:.service.in=.service)

EXTRA_DIST = \
	gcr-base.pc.in \
	gcr-marshal.list \
	gcr-oids.list \
	gcr-mkoids \
	$(DBUS_XML_DEFINITIONS) \
	gcr-base.symbols \
	$(service_in_files)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(pkgconfig_DATA) \
	$(service_DATA) \
	$(desktop_in_files) \
	$(gir_DATA) \
	$(typelibs_DATA) \
	gcr-base-actual.abi \
	gcr-base-expected.abi

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS = Gcr-@GCR_MAJOR@.gir
INTROSPECTION_SCANNER_ARGS = \
	$(INTROSPECTION_FLAGS) \
	--warn-all \
	--add-include-path=$(top_builddir)/gck  \
	--include-uninstalled=$(top_builddir)/gck/Gck-@GCK_MAJOR@.gir
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=$(top_builddir)/gck

Gcr-@GCR_MAJOR@.gir: libgcr-base-@GCR_MAJOR@.la

Gcr_@GCR_MAJOR@_gir_PACKAGES = p11-kit-1
Gcr_@GCR_MAJOR@_gir_EXPORT_PACKAGES = gcr-base-@GCR_MAJOR@
Gcr_@GCR_MAJOR@_gir_INCLUDES = GLib-2.0 GObject-2.0
Gcr_@GCR_MAJOR@_gir_LIBS = libgcr-base-@GCR_MAJOR@.la
Gcr_@GCR_MAJOR@_gir_CFLAGS = -I$(top_srcdir) -I$(top_builddir) \
	-DGCR_COMPILATION -DGCR_API_SUBJECT_TO_CHANGE
Gcr_@GCR_MAJOR@_gir_FILES = $(PUBLIC_FILES)
Gcr_@GCR_MAJOR@_gir_SCANNERFLAGS = --c-include "gcr/gcr-base.h" \
	--identifier-prefix Gcr

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

if ENABLE_VAPIGEN

include $(VAPIGEN_MAKEFILE)

gcr-@GCR_MAJOR@.vapi: Gcr-@GCR_MAJOR@.gir Gcr-@GCR_MAJOR@.metadata gcr-@GCR_MAJOR@.deps

VAPIGEN_VAPIS = gcr-@GCR_MAJOR@.vapi

gcr_@GCR_MAJOR@_vapi_DEPS = glib-2.0 gio-2.0 gck-@GCK_MAJOR@
gcr_@GCR_MAJOR@_vapi_METADATADIRS = $(srcdir)
gcr_@GCR_MAJOR@_vapi_VAPIDIRS = $(builddir)/../gck
gcr_@GCR_MAJOR@_vapi_GIRDIRS = $(builddir)/../gck
gcr_@GCR_MAJOR@_vapi_FILES = Gcr-@GCR_MAJOR@.gir

VAPI_DEPS = $(VAPIGEN_VAPIS:.vapi=.deps)

gcr-$(GCR_MAJOR).deps: Makefile.am
	$(AM_V_GEN) echo $(gcr_@GCR_MAJOR@_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/vala/vapi
vapi_DATA = \
	$(VAPIGEN_VAPIS) \
	$(VAPI_DEPS)

EXTRA_DIST += \
	Gcr-@GCR_MAJOR@.metadata

CLEANFILES += \
	$(VAPIGEN_VAPIS) \
	$(VAPI_DEPS)

endif # ENABLE_VAPIGEN

endif # HAVE_INTROSPECTON

# ----------------------------------------------------------------

gcr-base-expected.abi: gcr-base.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@

gcr-base-actual.abi: $(builddir)/.libs/libgcr-base-@GCR_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end|_gcr_)' | sort > $@

check-symbols: gcr-base-expected.abi gcr-base-actual.abi
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gcr-base-expected.abi gcr-base-actual.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gcr-base.symbols"; exit 1)
