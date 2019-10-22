
ui_incdir = $(includedir)/gcr-@GCR_MAJOR@/ui

check-ui-symbols: gcr-ui-expected.abi gcr-ui-actual.abi
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gcr-ui-expected.abi gcr-ui-actual.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gcr-ui.symbols"; exit 1)

ui_BUILT_SOURCES = \
	ui/gcr-enum-types.c ui/gcr-enum-types.h \
	ui/gcr-resources.c ui/gcr-resources.h

BUILT_SOURCES += \
	$(ui_BUILT_SOURCES)

libgcr_ui_@GCR_MAJOR@_la_CFLAGS = \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DP11_KIT_API_SUBJECT_TO_CHANGE \
	-DG_LOG_DOMAIN=\"Gcr\" \
	-DGCR_COMPILATION \
	$(P11_KIT_CFLAGS) \
	$(GTK_CFLAGS)

libgcr_ui_@GCR_MAJOR@_la_LDFLAGS = \
	-version-info $(GCR_LT_RELEASE) \
	-no-undefined \
	-export-symbols-regex '^_gcr_.*|^gcr_.*|^SECMEM_.*'

libgcr_ui_@GCR_MAJOR@_la_LIBADD = \
	libegg.la \
	libgcr-base-$(GCR_MAJOR).la \
	libgck-@GCK_MAJOR@.la \
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

lib_LTLIBRARIES += \
	libgcr-ui-@GCR_MAJOR@.la

ui_HEADER_FILES = \
	ui/gcr-ui.h \
	ui/gcr-certificate-basics-widget.h \
	ui/gcr-certificate-details-widget.h \
	ui/gcr-certificate-renderer.h \
	ui/gcr-certificate-widget.h \
	ui/gcr-collection-model.h \
	ui/gcr-combo-selector.h \
	ui/gcr-deprecated.h \
	ui/gcr-failure-renderer.h \
	ui/gcr-key-renderer.h \
	ui/gcr-key-widget.h \
	ui/gcr-import-button.h \
	ui/gcr-list-selector.h \
	ui/gcr-prompt-dialog.h \
	ui/gcr-renderer.h \
	ui/gcr-secure-entry-buffer.h \
	ui/gcr-tree-selector.h \
	ui/gcr-unlock-options-widget.h \
	ui/gcr-viewer.h \
	ui/gcr-viewer-widget.h

ui_inc_HEADERS = \
	$(ui_HEADER_FILES) \
	ui/gcr-enum-types.h

ui_PUBLIC_FILES = \
	ui/gcr-certificate-renderer.c ui/gcr-certificate-renderer.h \
	ui/gcr-certificate-widget.c ui/gcr-certificate-widget.h \
	ui/gcr-collection-model.c ui/gcr-collection-model.h \
	ui/gcr-combo-selector.c ui/gcr-combo-selector.h \
	ui/gcr-failure-renderer.c ui/gcr-failure-renderer.h \
	ui/gcr-key-renderer.c ui/gcr-key-renderer.h \
	ui/gcr-key-widget.c ui/gcr-key-widget.h \
	ui/gcr-import-button.c  ui/gcr-import-button.h \
	ui/gcr-list-selector.c ui/gcr-list-selector.h \
	ui/gcr-prompt-dialog.c ui/gcr-prompt-dialog.h \
	ui/gcr-renderer.c ui/gcr-renderer.h \
	ui/gcr-secure-entry-buffer.c ui/gcr-secure-entry-buffer.h \
	ui/gcr-tree-selector.c ui/gcr-tree-selector.h \
	ui/gcr-unlock-options-widget.c ui/gcr-unlock-options-widget.h \
	ui/gcr-viewer.c ui/gcr-viewer.h \
	ui/gcr-viewer-widget.c ui/gcr-viewer-widget.h \
	ui/gcr-ui.h \
	$(NULL)

ui_PRIVATE_FILES = \
	ui/eggimagemenuitem.c ui/eggimagemenuitem.h \
	ui/gcr-certificate-basics-widget.c ui/gcr-certificate-basics-widget.h \
	ui/gcr-certificate-details-widget.c ui/gcr-certificate-details-widget.h \
	ui/gcr-certificate-exporter.c ui/gcr-certificate-exporter.h \
	ui/gcr-certificate-renderer-private.h \
	ui/gcr-certificate-request-renderer.c ui/gcr-certificate-request-renderer.h \
	ui/gcr-deprecated.h \
	ui/gcr-dialog-util.c ui/gcr-dialog-util.h \
	ui/gcr-display-scrolled.c ui/gcr-display-scrolled.h \
	ui/gcr-display-view.c ui/gcr-display-view.h \
	ui/gcr-gnupg-renderer.c ui/gcr-gnupg-renderer.h \
	ui/gcr-list-selector-private.h \
	ui/gcr-live-search.c ui/gcr-live-search.h \
	ui/gcr-pkcs11-import-dialog.c ui/gcr-pkcs11-import-dialog.h \
	ui/gcr-pkcs11-import-interaction.c ui/gcr-pkcs11-import-interaction.h \
	ui/gcr-unlock-renderer.c ui/gcr-unlock-renderer.h \
	ui/gcr-viewer-window.c ui/gcr-viewer-window.h \
	$(NULL)

libgcr_ui_@GCR_MAJOR@_la_SOURCES = \
	$(ui_PUBLIC_FILES) \
	$(ui_PRIVATE_FILES) \
	$(NULL)

nodist_libgcr_ui_@GCR_MAJOR@_la_SOURCES = \
	$(ui_BUILT_SOURCES)

ui/gcr-enum-types.h: $(ENUM_TEMPLATE_H) $(ui_HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

ui/gcr-enum-types.c: $(ENUM_TEMPLATE_C) $(ui_HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

ui_RESOURCES = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/ui --generate-dependencies $(srcdir)/ui/gcr.gresource.xml)

ui/gcr-resources.h: ui/gcr.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/ui --generate-header --internal

ui/gcr-resources.c: ui/gcr.gresource.xml $(ui_RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ --sourcedir=$(srcdir)/ui --generate-source --internal

gcr-ui-$(GCR_MAJOR).pc: ui/gcr-ui.pc
	$(AM_V_GEN) cp ui/gcr-ui.pc gcr-ui-$(GCR_MAJOR).pc
gcr-$(GCR_MAJOR).pc: ui/gcr.pc
	$(AM_V_GEN) cp ui/gcr.pc gcr-$(GCR_MAJOR).pc

if HAVE_INTROSPECTION

INTROSPECTION_GIRS += GcrUi-@GCR_MAJOR@.gir

GcrUi-@GCR_MAJOR@.gir: libgcr-ui-@GCR_MAJOR@.la Gcr-@GCR_MAJOR@.gir

GcrUi_@GCR_MAJOR@_gir_PACKAGES = gtk+-3.0 p11-kit-1
GcrUi_@GCR_MAJOR@_gir_EXPORT_PACKAGES = gcr-ui-@GCR_MAJOR@
GcrUi_@GCR_MAJOR@_gir_INCLUDES = GLib-2.0 GObject-2.0 Gtk-3.0
GcrUi_@GCR_MAJOR@_gir_LIBS = libgcr-base-@GCR_MAJOR@.la libgcr-ui-@GCR_MAJOR@.la
GcrUi_@GCR_MAJOR@_gir_CFLAGS = -I$(srcdir) -I$(builddir) -DGCR_COMPILATION -DGCR_API_SUBJECT_TO_CHANGE
GcrUi_@GCR_MAJOR@_gir_FILES = $(ui_PUBLIC_FILES)
GcrUi_@GCR_MAJOR@_gir_SCANNERFLAGS = \
	--add-include-path=$(builddir)/gck \
	--add-include-path=$(builddir)/gcr \
	--include-uninstalled=Gck-@GCK_MAJOR@.gir \
	--include-uninstalled=Gcr-@GCR_MAJOR@.gir \
	--c-include "ui/gcr-ui.h" \
	--identifier-prefix Gcr --symbol-prefix gcr

gir_DATA += GcrUi-@GCR_MAJOR@.gir

if ENABLE_VAPIGEN

gcr-ui-@GCR_MAJOR@.vapi: GcrUi-@GCR_MAJOR@.gir ui/GcrUi-@GCR_MAJOR@.metadata gcr-ui-@GCR_MAJOR@.deps gck-@GCK_MAJOR@.vapi gcr-@GCR_MAJOR@.vapi

VAPIGEN_VAPIS += gcr-ui-@GCR_MAJOR@.vapi

gcr_ui_@GCR_MAJOR@_vapi_DEPS = glib-2.0 gio-2.0 gck-@GCK_MAJOR@ gcr-@GCR_MAJOR@ gtk+-3.0
gcr_ui_@GCR_MAJOR@_vapi_METADATADIRS = $(srcdir)/ui
gcr_ui_@GCR_MAJOR@_vapi_VAPIDIRS = $(builddir)
gcr_ui_@GCR_MAJOR@_vapi_GIRDIRS = $(builddir)
gcr_ui_@GCR_MAJOR@_vapi_FILES = GcrUi-@GCR_MAJOR@.gir

gcr-ui-$(GCR_MAJOR).deps: Makefile.am
	$(AM_V_GEN) echo $(gcr_ui_@GCR_MAJOR@_vapi_DEPS) | tr ' ' '\n' > $@

EXTRA_DIST += \
	ui/GcrUi-@GCR_MAJOR@.metadata

endif # ENABLE_VAPIGEN

endif # HAVE_INTROSPECTION

pkgconfig_DATA += \
	gcr-$(GCR_MAJOR).pc \
	gcr-ui-$(GCR_MAJOR).pc

gcr-ui-expected.abi: ui/gcr-ui.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@

bin_PROGRAMS = gcr-viewer

gcr_viewer_SOURCES = \
	ui/gcr-viewer-tool.c

gcr_viewer_CFLAGS = \
	$(P11_KIT_CFLAGS) \
	$(GTK_CFLAGS) \
	-DGCR_API_SUBJECT_TO_CHANGE

gcr_viewer_LDADD = \
	libgcr-ui-$(GCR_MAJOR).la \
	libgcr-base-$(GCR_MAJOR).la \
	libgck-@GCK_MAJOR@.la \
	$(GLIB_LIBS) \
	$(GTK_LIBS)

viewer_desktop_in_in = ui/gcr-viewer.desktop.in.in
viewer_desktop_in = $(viewer_desktop_in_in:.desktop.in.in=.desktop.in)
viewer_desktop = $(viewer_desktop_in:.desktop.in=.desktop)
$(viewer_desktop_in): $(viewer_desktop_in_in)
	$(AM_V_GEN) sed 's|@bindir[@]|$(bindir)|g' $< > $@
$(viewer_desktop): $(viewer_desktop_in)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

libexec_PROGRAMS += gcr-prompter

gcr_prompter_SOURCES = \
	ui/gcr-prompter-tool.c

gcr_prompter_CFLAGS = \
	-DGCR_API_SUBJECT_TO_CHANGE \
	$(P11_KIT_CFLAGS) \
	$(GTK_CFLAGS)

gcr_prompter_LDADD = \
	libgcr-ui-$(GCR_MAJOR).la \
	libgcr-base-$(GCR_MAJOR).la \
	libgck-@GCK_MAJOR@.la \
	$(GTK_LIBS)

prompter_desktop_in_in = ui/gcr-prompter.desktop.in.in
prompter_desktop_in = $(prompter_desktop_in_in:.desktop.in.in=.desktop.in)
prompter_desktop = $(prompter_desktop_in:.desktop.in=.desktop)
$(prompter_desktop_in): $(prompter_desktop_in_in)
	$(AM_V_GEN) sed 's|@libexecdir[@]|$(libexecdir)|g' $< > $@
$(prompter_desktop): $(prompter_desktop_in)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

desktop_in_in_files = $(viewer_desktop_in_in) $(prompter_desktop_in_in)
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktopdir = $(datadir)/applications
desktop_DATA = \
	$(prompter_desktop) \
	$(viewer_desktop)

EXTRA_DIST += \
	ui/gcr.pc.in \
	ui/gcr-ui.pc.in \
	ui/gcr-ui.symbols \
	ui/gcr.gresource.xml \
	$(ui_RESOURCES) \
	$(mime_DATA) \
	$(desktop_in_in_files) \
	ui/fixtures

CLEANFILES += \
	$(desktop_DATA) \
	gcr-ui-actual.abi \
	gcr-ui-expected.abi \
	$(desktop_in_files) \
	$(NULL)

# The new mime system
mimedir = $(datadir)/mime/packages
mime_DATA = ui/gcr-crypto-types.xml

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

gcr-ui-actual.abi: .libs/libgcr-ui-@GCR_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end|_gcr_)' | sort > $@

ui_CFLAGS = \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION \
	$(LIBGCRYPT_CFLAGS) \
	$(P11_KIT_CFLAGS) \
	$(GTK_CFLAGS)

ui_LIBS = \
	libgcr-ui-@GCR_MAJOR@.la \
	libgcr-base-@GCR_MAJOR@.la \
	libegg.la \
	libgck-testable.la \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS) \
	$(GTK_LIBS)

# ------------------------------------------------------------------

noinst_PROGRAMS += \
	frob-certificate \
	frob-combo-selector \
	frob-gnupg-selector \
	frob-import-button \
	frob-key \
	frob-tree-selector \
	frob-prompt \
	frob-request \
	frob-system-prompt \
	frob-unlock \
	frob-unlock-options

frob_certificate_SOURCES = ui/frob-certificate.c
frob_certificate_CFLAGS = $(ui_CFLAGS)
frob_certificate_LDADD = $(ui_LIBS)

frob_combo_selector_SOURCES = ui/frob-combo-selector.c
frob_combo_selector_CFLAGS = $(ui_CFLAGS)
frob_combo_selector_LDADD = $(ui_LIBS)

frob_gnupg_selector_SOURCES = ui/frob-gnupg-selector.c
frob_gnupg_selector_CFLAGS = $(ui_CFLAGS)
frob_gnupg_selector_LDADD = $(ui_LIBS)

frob_import_button_SOURCES = ui/frob-import-button.c
frob_import_button_CFLAGS = $(ui_CFLAGS)
frob_import_button_LDADD = $(ui_LIBS)

frob_key_SOURCES = ui/frob-key.c
frob_key_CFLAGS = $(ui_CFLAGS)
frob_key_LDADD = $(ui_LIBS)

frob_prompt_SOURCES = ui/frob-prompt.c
frob_prompt_CFLAGS = $(ui_CFLAGS)
frob_prompt_LDADD = $(ui_LIBS)

frob_request_SOURCES = ui/frob-request.c
frob_request_CFLAGS = $(ui_CFLAGS)
frob_request_LDADD = $(ui_LIBS)

frob_system_prompt_SOURCES = ui/frob-system-prompt.c
frob_system_prompt_CFLAGS = $(ui_CFLAGS)
frob_system_prompt_LDADD = $(ui_LIBS)

frob_tree_selector_SOURCES = ui/frob-tree-selector.c
frob_tree_selector_CFLAGS = $(ui_CFLAGS)
frob_tree_selector_LDADD = $(ui_LIBS)

frob_unlock_SOURCES = \
	ui/frob-unlock.c ui/gcr-viewer-window.c
frob_unlock_CFLAGS = $(ui_CFLAGS)
frob_unlock_LDADD = $(ui_LIBS)

frob_unlock_options_SOURCES = ui/frob-unlock-options.c
frob_unlock_options_CFLAGS = $(ui_CFLAGS)
frob_unlock_options_LDADD = $(ui_LIBS)
