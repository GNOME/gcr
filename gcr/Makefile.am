include $(top_srcdir)/Makefile.decl

SUBDIRS = . icons tests

# ------------------------------------------------------------------
# UI BUILDER
#

uidir = $(datadir)/gcr-@GCR_MAJOR@/ui/

ui_DATA = \
	gcr-pkcs11-import-dialog.ui \
	gcr-unlock-options-widget.ui

# ------------------------------------------------------------------
# HEADERS

HEADER_BASE_FILES = \
	gcr-base.h \
	gcr-certificate.h \
	gcr-certificate-chain.h \
	gcr-certificate-request.h \
	gcr-collection.h \
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
	gcr-system-prompt.h \
	gcr-system-prompter.h \
	gcr-trust.h \
	gcr-types.h \
	gcr-union-collection.h \
	gcr-unlock-options.h

HEADER_UI_FILES = \
	gcr.h \
	gcr-certificate-basics-widget.h \
	gcr-certificate-details-widget.h \
	gcr-certificate-renderer.h \
	gcr-certificate-widget.h \
	gcr-collection-model.h \
	gcr-column.h \
	gcr-combo-selector.h \
	gcr-deprecated.h \
	gcr-failure-renderer.h \
	gcr-key-renderer.h \
	gcr-key-widget.h \
	gcr-import-button.h \
	gcr-list-selector.h \
	gcr-prompt-dialog.h \
	gcr-renderer.h \
	gcr-secure-entry-buffer.h \
	gcr-simple-collection.h \
	gcr-tree-selector.h \
	gcr-unlock-options-widget.h \
	gcr-viewer.h \
	gcr-viewer-widget.h

incdir = $(includedir)/gcr-@GCR_MAJOR@/gcr

inc_HEADERS = \
	$(HEADER_BASE_FILES) \
	$(HEADER_UI_FILES) \
	gcr-enum-types-base.h \
	gcr-enum-types.h

# ------------------------------------------------------------------
# LIBRARY

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(P11_KIT_CFLAGS) \
	-DG_LOG_DOMAIN=\"Gcr\" \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DLOCALEDIR=\""$(datadir)/locale"\"

lib_LTLIBRARIES = \
	libgcr-base-@GCR_MAJOR@.la \
	libgcr-@GCR_MAJOR@.la

BUILT_BASE_FILES = \
	gcr-marshal.c gcr-marshal.h \
	gcr-enum-types-base.c gcr-enum-types-base.h \
	gcr-oids.c gcr-oids.h \
	gcr-dbus-generated.c gcr-dbus-generated.h

BUILT_UI_FILES = \
	gcr-marshal.c gcr-marshal.h \
	gcr-enum-types.c gcr-enum-types.h \
	gcr-oids.c gcr-oids.h

BUILT_SOURCES = \
	$(BUILT_BASE_FILES) \
	$(BUILT_UI_FILES)

libgcr_base_@GCR_MAJOR@_la_SOURCES = \
	gcr-callback-output-stream.c gcr-callback-output-stream.h \
	gcr-certificate.c gcr-certificate.h \
	gcr-certificate-chain.c gcr-certificate-chain.h \
	gcr-certificate-extensions.c gcr-certificate-extensions.h \
	gcr-certificate-request.c gcr-certificate-request.h \
	gcr-collection.c gcr-collection.h \
	gcr-comparable.c gcr-comparable.h \
	gcr-dbus-constants.h \
	gcr-debug.c gcr-debug.h \
	gcr-filter-collection.c gcr-filter-collection.h \
	gcr-fingerprint.c gcr-fingerprint.h \
	gcr-gnupg-collection.c gcr-gnupg-collection.h \
	gcr-gnupg-importer.c gcr-gnupg-importer.h \
	gcr-gnupg-key.c gcr-gnupg-key.h \
	gcr-gnupg-process.c gcr-gnupg-process.h \
	gcr-gnupg-records.c gcr-gnupg-records.h \
	gcr-gnupg-util.c gcr-gnupg-util.h \
	gcr-icons.c gcr-icons.h \
	gcr-importer.c gcr-importer.h \
	gcr-import-interaction.c gcr-import-interaction.h \
	gcr-internal.h \
	gcr-key-mechanisms.h gcr-key-mechanisms.c \
	gcr-library.c gcr-library.h \
	gcr-memory.c \
	gcr-memory-icon.c gcr-memory-icon.h \
	gcr-mock-prompter.c gcr-mock-prompter.h \
	gcr-openpgp.c gcr-openpgp.h \
	gcr-openssh.c gcr-openssh.h \
	gcr-parser.c gcr-parser.h \
	gcr-pkcs11-certificate.c gcr-pkcs11-certificate.h \
	gcr-pkcs11-importer.c gcr-pkcs11-importer.h \
	gcr-prompt.c gcr-prompt.h \
	gcr-record.c gcr-record.h \
	gcr-secret-exchange.c gcr-secret-exchange.h \
	gcr-secure-memory.c gcr-secure-memory.h \
	gcr-simple-certificate.c gcr-simple-certificate.h \
	gcr-simple-collection.c gcr-simple-collection.h \
	gcr-single-collection.c gcr-single-collection.h \
	gcr-subject-public-key.c gcr-subject-public-key.h \
	gcr-system-prompt.c gcr-system-prompt.h \
	gcr-system-prompter.c gcr-system-prompter.h \
	gcr-trust.c gcr-trust.h \
	gcr-types.h \
	gcr-union-collection.c gcr-union-collection.h \
	gcr-util.c gcr-util.h \
	$(BUILT_BASE_FILES)

libgcr_@GCR_MAJOR@_la_SOURCES = \
	gcr-certificate-basics-widget.c gcr-certificate-basics-widget.h \
	gcr-certificate-details-widget.c gcr-certificate-details-widget.h \
	gcr-certificate-exporter.c gcr-certificate-exporter.h \
	gcr-certificate-extensions.c gcr-certificate-extensions.h \
	gcr-certificate-renderer.c gcr-certificate-renderer.h gcr-certificate-renderer-private.h \
	gcr-certificate-request-renderer.c gcr-certificate-request-renderer.h \
	gcr-certificate-widget.c gcr-certificate-widget.h \
	gcr-collection-model.c gcr-collection-model.h \
	gcr-combo-selector.c gcr-combo-selector.h \
	gcr-debug.c gcr-debug.h \
	gcr-dialog-util.c gcr-dialog-util.h \
	gcr-display-scrolled.c gcr-display-scrolled.h \
	gcr-display-view.c gcr-display-view.h \
	gcr-failure-renderer.c gcr-failure-renderer.h \
	gcr-gnupg-renderer.c gcr-gnupg-renderer.h \
	gcr-gnupg-records.c gcr-gnupg-records.h \
	gcr-import-button.c gcr-import-button.h \
	gcr-key-renderer.c gcr-key-renderer.h \
	gcr-key-widget.c gcr-key-widget.h \
	gcr-list-selector.c gcr-list-selector.h gcr-list-selector-private.h \
	gcr-live-search.c gcr-live-search.h \
	gcr-memory.c \
	gcr-memory-icon.c gcr-memory-icon.h \
	gcr-pkcs11-import-interaction.c gcr-pkcs11-import-interaction.h \
	gcr-pkcs11-import-dialog.c gcr-pkcs11-import-dialog.h \
	gcr-prompt-dialog.c gcr-prompt-dialog.h \
	gcr-record.c gcr-record.h \
	gcr-renderer.c gcr-renderer.h \
	gcr-secure-entry-buffer.c gcr-secure-entry-buffer.h \
	gcr-subject-public-key.c gcr-subject-public-key.h \
	gcr-tree-selector.c gcr-tree-selector.h \
	gcr-unlock-options.h \
	gcr-unlock-options-widget.c gcr-unlock-options-widget.h \
	gcr-unlock-renderer.c gcr-unlock-renderer.h \
	gcr-viewer.c gcr-viewer.h \
	gcr-viewer-widget.c gcr-viewer-widget.h \
	gcr-viewer-window.c gcr-viewer-window.h \
	$(BUILT_UI_FILES)

libgcr_base_@GCR_MAJOR@_la_CFLAGS = \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DP11_KIT_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION

libgcr_@GCR_MAJOR@_la_CFLAGS = \
	$(GTK_CFLAGS) \
	-DUIDIR=\""$(uidir)"\" \
	$(libgcr_base_@GCR_MAJOR@_la_CFLAGS)

libgcr_base_@GCR_MAJOR@_la_LDFLAGS = \
	-version-info $(GCR_LT_RELEASE) \
	-no-undefined \
	-export-symbols-regex '^gcr_.*|^SECMEM_.*'

libgcr_@GCR_MAJOR@_la_LDFLAGS = \
	$(libgcr_base_@GCR_MAJOR@_la_LDFLAGS)

libgcr_base_@GCR_MAJOR@_la_LIBADD = \
	$(top_builddir)/egg/libegg.la \
	$(top_builddir)/gck/libgck-@GCK_MAJOR@.la \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS)

libgcr_@GCR_MAJOR@_la_LIBADD = \
	$(GTK_LIBS) \
	$(libgcr_base_@GCR_MAJOR@_la_LIBADD) \
	$(builddir)/libgcr-base-$(GCR_MAJOR).la

noinst_LTLIBRARIES = libgcr-testable.la
libgcr_testable_la_SOURCES =
libgcr_testable_la_LIBADD = \
	$(libgcr_base_@GCR_MAJOR@_la_OBJECTS) \
	$(libgcr_@GCR_MAJOR@_la_OBJECTS) \
	$(libgcr_base_@GCR_MAJOR@_la_LIBADD)
libgcr_testable_la_DEPENDENCIES = \
	$(libgcr_base_@GCR_MAJOR@_la_OBJECTS) \
	$(libgcr_@GCR_MAJOR@_la_OBJECTS)

gcr-marshal.h: gcr-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=_gcr_marshal > $@

gcr-marshal.c: gcr-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) ( echo "#include \"gcr-marshal.h\"" > $@ && \
		$(GLIB_GENMARSHAL) $< --body --prefix=_gcr_marshal >> $@ )

gcr-enum-types.h: gcr-enum-types.h.template $(HEADER_UI_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gcr-enum-types.c: gcr-enum-types.c.template $(HEADER_UI_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gcr-enum-types-base.h: gcr-enum-types.h.template $(HEADER_BASE_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gcr-enum-types-base.c: gcr-enum-types.c.template $(HEADER_BASE_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

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

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gcr-$(GCR_MAJOR).pc gcr-base-$(GCR_MAJOR).pc

gcr-$(GCR_MAJOR).pc: gcr.pc
	cp gcr.pc gcr-$(GCR_MAJOR).pc
gcr-base-$(GCR_MAJOR).pc: gcr-base.pc
	cp gcr-base.pc gcr-base-$(GCR_MAJOR).pc

# ----------------------------------------------------------------
# SYSTEM INTEGRATION

# The new mime system
mimedir = $(datadir)/mime/packages
mime_DATA = gcr-crypto-types.xml

if WITH_UPDATE_MIME
update_mime_database_cmd = update-mime-database $(datadir)/mime/

install-data-hook: update_mime_database
uninstall-hook: update_mime_database

update_mime_database:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating MIME database."; \
		$(update_mime_database_cmd); \
	else \
		echo "*** MIME database not updated.  After (un)install, run this:"; \
		echo "***   $(update_mime_database_cmd)"; \
	fi
endif

desktopdir = $(datadir)/applications
desktop_in_in_files = gcr-viewer.desktop.in.in gcr-prompter.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

service_in_files = \
	org.gnome.keyring.SystemPrompter.service.in \
	org.gnome.keyring.PrivatePrompter.service.in
servicedir       = $(DBUS_SERVICES_DIR)
service_DATA     = $(service_in_files:.service.in=.service)

# ----------------------------------------------------------------
# TOOLS

bin_PROGRAMS = gcr-viewer

gcr_viewer_SOURCES = \
	gcr-viewer-tool.c

gcr_viewer_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(datadir)/locale"\" \
	-DGCR_API_SUBJECT_TO_CHANGE

gcr_viewer_LDADD = \
	$(builddir)/libgcr-$(GCR_MAJOR).la \
	$(builddir)/libgcr-base-$(GCR_MAJOR).la \
	$(top_builddir)/gck/libgck-@GCK_MAJOR@.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS)

libexec_PROGRAMS = gcr-prompter

gcr_prompter_SOURCES = \
	gcr-prompter-tool.c \
	gcr-debug.c gcr-debug.h

gcr_prompter_CFLAGS = \
	$(GTK_CFLAGS)

gcr_prompter_LDADD = \
	$(builddir)/libgcr-$(GCR_MAJOR).la \
	$(builddir)/libgcr-base-$(GCR_MAJOR).la \
	$(top_builddir)/gck/libgck-@GCK_MAJOR@.la \
	$(GTK_LIBS)

# ------------------------------------------------------------------
# INTROSPECTION

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

Gcr-@GCR_MAJOR@.gir: libgcr-@GCR_MAJOR@.la

Gcr_@GCR_MAJOR@_gir_PACKAGES = gtk+-3.0 p11-kit-1
Gcr_@GCR_MAJOR@_gir_EXPORT_PACKAGES = gcr-@GCR_MAJOR@
Gcr_@GCR_MAJOR@_gir_INCLUDES = GLib-2.0 GObject-2.0 Gtk-3.0
Gcr_@GCR_MAJOR@_gir_LIBS = libgcr-base-@GCR_MAJOR@.la libgcr-@GCR_MAJOR@.la
Gcr_@GCR_MAJOR@_gir_CFLAGS = -I$(top_srcdir) -I$(top_builddir) -DGCR_COMPILATION -DGCR_API_SUBJECT_TO_CHANGE
Gcr_@GCR_MAJOR@_gir_FILES = $(libgcr_base_@GCR_MAJOR@_la_SOURCES) $(libgcr_@GCR_MAJOR@_la_SOURCES)
Gcr_@GCR_MAJOR@_gir_SCANNERFLAGS = --c-include "gcr.h" --c-include "gcr-base.h"

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

endif

# ----------------------------------------------------------------

gcr-expected-base.abi: gcr-base.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@
gcr-expected.abi: gcr.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@

gcr-actual-base.abi: $(builddir)/.libs/libgcr-base-@GCR_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end)' | sort > $@
gcr-actual.abi: $(builddir)/.libs/libgcr-@GCR_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end)' | sort > $@

check-symbols: gcr-expected.abi gcr-actual.abi gcr-expected-base.abi gcr-actual-base.abi
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gcr-expected-base.abi gcr-actual-base.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gcr-base.symbols"; exit 1)
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gcr-expected.abi gcr-actual.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gcr.symbols"; exit 1)

check-local: check-symbols

check-memory:
	make -C tests check-memory

EXTRA_DIST = \
	gcr.pc.in \
	gcr-base.pc.in \
	gcr-marshal.list \
	gcr-oids.list \
	gcr-mkoids \
	$(ui_DATA) \
	$(conf_DATA) \
	org.gnome.keyring.Prompter.xml \
	gcr-enum-types.h.template \
	gcr-enum-types.c.template \
	gcr.symbols \
	gcr-base.symbols \
	$(desktop_in_in_files) \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(service_in_files) \
	$(mime_DATA)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(pkgconfig_DATA) \
	$(service_DATA) \
	$(desktop_in_files) \
	$(desktop_DATA) \
	gcr-actual.abi \
	gcr-actual-base.abi \
	gcr-expected.abi \
	gcr-expected-base.abi

DISTCLEANFILES = \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(pkgconfig_DATA) \
	$(gir_DATA) \
	$(typelibs_DATA)
