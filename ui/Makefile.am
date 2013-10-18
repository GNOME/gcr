include $(top_srcdir)/Makefile.decl

SUBDIRS = . icons tests

incdir = $(includedir)/gcr-@GCR_MAJOR@/ui

EXTRA_DIST = 
CLEANFILES =

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

check-symbols: gcr-ui-expected.abi gcr-ui-actual.abi
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gcr-ui-expected.abi gcr-ui-actual.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gcr-ui.symbols"; exit 1)

BUILT_SOURCES = \
	gcr-enum-types.c gcr-enum-types.h

libgcr_ui_@GCR_MAJOR@_la_CFLAGS = \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DP11_KIT_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION \
	$(GTK_CFLAGS) \
	-DUIDIR=\""$(uidir)"\"

libgcr_ui_@GCR_MAJOR@_la_LDFLAGS = \
	-version-info $(GCR_LT_RELEASE) \
	-no-undefined \
	-export-symbols-regex '^_gcr_.*|^gcr_.*|^SECMEM_.*'

libgcr_ui_@GCR_MAJOR@_la_LIBADD = \
	$(top_builddir)/egg/libegg.la \
	$(top_builddir)/gcr/libgcr-base-$(GCR_MAJOR).la \
	$(top_builddir)/gck/libgck-@GCK_MAJOR@.la \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS) \
	$(GTK_LIBS) \
	$(NULL)

# libgcr-3.so (and  friends) symlink to libgcr-ui-3.so for compatibility raisons
install-exec-hook:
	$(LN_S) -f `readlink $(DESTDIR)$(libdir)/libgcr-ui-@GCR_MAJOR@.so` $(DESTDIR)$(libdir)/libgcr-@GCR_MAJOR@.so
	$(LN_S) -f `readlink $(DESTDIR)$(libdir)/libgcr-ui-@GCR_MAJOR@.so` $(DESTDIR)$(libdir)/libgcr-@GCR_MAJOR@.so.@GCR_CURRENT@
	$(LN_S) -f `readlink $(DESTDIR)$(libdir)/libgcr-ui-@GCR_MAJOR@.so` $(DESTDIR)$(libdir)/libgcr-@GCR_MAJOR@.so.@GCR_CURRENT@.0.0

uidir = $(datadir)/gcr-@GCR_MAJOR@/ui/

ui_DATA = \
	gcr-pkcs11-import-dialog.ui \
	gcr-unlock-options-widget.ui

lib_LTLIBRARIES = \
	libgcr-ui-@GCR_MAJOR@.la

HEADER_FILES = \
	gcr-ui.h \
	gcr-certificate-basics-widget.h \
	gcr-certificate-details-widget.h \
	gcr-certificate-renderer.h \
	gcr-certificate-widget.h \
	gcr-collection-model.h \
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
	gcr-tree-selector.h \
	gcr-unlock-options-widget.h \
	gcr-viewer.h \
	gcr-viewer-widget.h

inc_HEADERS = \
	$(HEADER_FILES) \
	gcr-enum-types.h

PUBLIC_FILES = \
	gcr-certificate-renderer.c gcr-certificate-renderer.h \
	gcr-certificate-widget.c gcr-certificate-widget.h \
	gcr-collection-model.c gcr-collection-model.h \
	gcr-combo-selector.c gcr-combo-selector.h \
	gcr-failure-renderer.c gcr-failure-renderer.h \
	gcr-key-renderer.c gcr-key-renderer.h \
	gcr-key-widget.c gcr-key-widget.h \
	gcr-import-button.c  gcr-import-button.h \
	gcr-list-selector.c gcr-list-selector.h \
	gcr-prompt-dialog.c gcr-prompt-dialog.h \
	gcr-renderer.c gcr-renderer.h \
	gcr-secure-entry-buffer.c gcr-secure-entry-buffer.h \
	gcr-tree-selector.c gcr-tree-selector.h \
	gcr-unlock-options-widget.c gcr-unlock-options-widget.h \
	gcr-viewer.c gcr-viewer.h \
	gcr-viewer-widget.c gcr-viewer-widget.h \
	gcr-ui.h \
	$(NULL)

PRIVATE_FILES = \
	gcr-certificate-basics-widget.c gcr-certificate-basics-widget.h \
	gcr-certificate-details-widget.c gcr-certificate-details-widget.h \
	gcr-certificate-exporter.c gcr-certificate-exporter.h \
	gcr-certificate-renderer-private.h \
	gcr-certificate-request-renderer.c gcr-certificate-request-renderer.h \
	gcr-deprecated.h \
	gcr-dialog-util.c gcr-dialog-util.h \
	gcr-display-scrolled.c gcr-display-scrolled.h \
	gcr-display-view.c gcr-display-view.h \
	gcr-gnupg-renderer.c gcr-gnupg-renderer.h \
	gcr-list-selector-private.h \
	gcr-live-search.c gcr-live-search.h \
	gcr-pkcs11-import-dialog.c gcr-pkcs11-import-dialog.h \
	gcr-pkcs11-import-interaction.c gcr-pkcs11-import-interaction.h \
	gcr-unlock-renderer.c gcr-unlock-renderer.h \
	gcr-viewer-window.c gcr-viewer-window.h \
	$(NULL)

libgcr_ui_@GCR_MAJOR@_la_SOURCES = \
	$(PUBLIC_FILES) \
	$(PRIVATE_FILES) \
	$(NULL)

nodist_libgcr_ui_@GCR_MAJOR@_la_SOURCES = \
	$(BUILT_SOURCES)

gcr-enum-types.h: $(ENUM_TEMPLATE_H) $(HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gcr-enum-types.c: $(ENUM_TEMPLATE_C) $(HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gcr-ui-$(GCR_MAJOR).pc: gcr-ui.pc
	$(AM_V_GEN) cp gcr-ui.pc gcr-ui-$(GCR_MAJOR).pc
gcr-$(GCR_MAJOR).pc: gcr.pc
	$(AM_V_GEN) cp gcr.pc gcr-$(GCR_MAJOR).pc

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS = GcrUi-@GCR_MAJOR@.gir
INTROSPECTION_SCANNER_ARGS = \
	$(INTROSPECTION_FLAGS) \
	--warn-all \
	--add-include-path=$(top_builddir)/gck \
	--add-include-path=$(top_builddir)/gcr \
	--include-uninstalled=$(top_builddir)/gck/Gck-@GCK_MAJOR@.gir \
	--include-uninstalled=$(top_builddir)/gcr/Gcr-@GCR_MAJOR@.gir \
	$(NULL)

INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=$(top_builddir)/gck \
	--includedir=$(top_builddir)/gcr \
	$(NULL)

GcrUi-@GCR_MAJOR@.gir: $(builddir)/libgcr-ui-@GCR_MAJOR@.la $(top_builddir)/gcr/Gcr-@GCR_MAJOR@.gir

GcrUi_@GCR_MAJOR@_gir_PACKAGES = gtk+-3.0 p11-kit-1
GcrUi_@GCR_MAJOR@_gir_EXPORT_PACKAGES = gcr-ui-@GCR_MAJOR@
GcrUi_@GCR_MAJOR@_gir_INCLUDES = GLib-2.0 GObject-2.0 Gtk-3.0
GcrUi_@GCR_MAJOR@_gir_LIBS = $(top_builddir)/gcr/libgcr-base-@GCR_MAJOR@.la libgcr-ui-@GCR_MAJOR@.la
GcrUi_@GCR_MAJOR@_gir_CFLAGS = -I$(top_srcdir) -I$(top_builddir) -DGCR_COMPILATION -DGCR_API_SUBJECT_TO_CHANGE
GcrUi_@GCR_MAJOR@_gir_FILES = $(PUBLIC_FILES)
GcrUi_@GCR_MAJOR@_gir_SCANNERFLAGS = --c-include "ui/gcr-ui.h" \
	--identifier-prefix Gcr --symbol-prefix gcr \
	--include-uninstalled=$(top_builddir)/gcr/Gcr-@GCR_MAJOR@.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

if ENABLE_VAPIGEN
include $(VAPIGEN_MAKEFILE)

gcr-ui-@GCR_MAJOR@.vapi: GcrUi-@GCR_MAJOR@.gir GcrUi-@GCR_MAJOR@.metadata gcr-ui-@GCR_MAJOR@.deps

VAPIGEN_VAPIS = gcr-ui-@GCR_MAJOR@.vapi

gcr_ui_@GCR_MAJOR@_vapi_DEPS = glib-2.0 gio-2.0 gck-@GCK_MAJOR@ gcr-@GCR_MAJOR@ gtk+-3.0
gcr_ui_@GCR_MAJOR@_vapi_METADATADIRS = $(srcdir)
gcr_ui_@GCR_MAJOR@_vapi_VAPIDIRS = $(builddir) $(builddir)/../gck $(builddir)/../gcr
gcr_ui_@GCR_MAJOR@_vapi_GIRDIRS = $(builddir) $(builddir)/../gck $(builddir)/../gcr
gcr_ui_@GCR_MAJOR@_vapi_FILES = GcrUi-@GCR_MAJOR@.gir

VAPI_DEPS = $(VAPIGEN_VAPIS:.vapi=.deps)

gcr-ui-$(GCR_MAJOR).deps: Makefile.am
	$(AM_V_GEN) echo $(gcr_ui_@GCR_MAJOR@_vapi_DEPS) | tr ' ' '\n' > $@

vapidir = $(datadir)/vala/vapi
vapi_DATA = \
	$(VAPIGEN_VAPIS) \
	$(VAPI_DEPS)

EXTRA_DIST += \
	GcrUi-@GCR_MAJOR@.metadata

CLEANFILES += \
	$(VAPIGEN_VAPIS) \
	$(VAPI_DEPS)

endif # ENABLE_VAPIGEN

endif # HAVE_INTROSPECTION

pkgconfig_DATA = \
	gcr-$(GCR_MAJOR).pc \
	gcr-ui-$(GCR_MAJOR).pc

gcr-ui-expected.abi: gcr-ui.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@

desktopdir = $(datadir)/applications
desktop_in_in_files = gcr-viewer.desktop.in.in gcr-prompter.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

bin_PROGRAMS = gcr-viewer

gcr_viewer_SOURCES = \
	gcr-viewer-tool.c

gcr_viewer_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	-DLOCALEDIR=\""$(datadir)/locale"\" \
	-DGCR_API_SUBJECT_TO_CHANGE

gcr_viewer_LDADD = \
	$(builddir)/libgcr-ui-$(GCR_MAJOR).la \
	$(top_builddir)/gcr/libgcr-base-$(GCR_MAJOR).la \
	$(top_builddir)/gck/libgck-@GCK_MAJOR@.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS)

libexec_PROGRAMS = gcr-prompter

gcr_prompter_SOURCES = \
	gcr-prompter-tool.c

gcr_prompter_CFLAGS = \
	$(GTK_CFLAGS)

gcr_prompter_LDADD = \
	$(builddir)/libgcr-ui-$(GCR_MAJOR).la \
	$(top_builddir)/gcr/libgcr-base-$(GCR_MAJOR).la \
	$(top_builddir)/gck/libgck-@GCK_MAJOR@.la \
	$(GTK_LIBS)

EXTRA_DIST += \
	gcr.pc.in \
	gcr-ui.pc.in \
	$(ui_DATA) \
	gcr-ui.symbols \
	$(desktop_in_in_files) \
	$(mime_DATA)

CLEANFILES += \
	$(desktop_DATA) \
	gcr-ui-actual.abi \
	gcr-ui-expected.abi \
	$(BUILT_SOURCES) \
	$(pkgconfig_DATA) \
	$(gir_DATA) \
	$(typelibs_DATA) \
	$(NULL)

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

endif # WITH_UPDATE_MIME

gcr-ui-actual.abi: $(builddir)/.libs/libgcr-ui-@GCR_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end|_gcr_)' | sort > $@
