# included in top-level Makefile.am

gcr_incdir = $(includedir)/gcr-@GCR_MAJOR@/gcr

gcr_HEADER_FILES = \
	gcr/gcr-base.h \
	gcr/gcr-certificate.h \
	gcr/gcr-certificate-chain.h \
	gcr/gcr-certificate-request.h \
	gcr/gcr-collection.h \
	gcr/gcr-column.h \
	gcr/gcr-comparable.h \
	gcr/gcr-deprecated-base.h \
	gcr/gcr-fingerprint.h \
	gcr/gcr-filter-collection.h \
	gcr/gcr-icons.h \
	gcr/gcr-importer.h \
	gcr/gcr-import-interaction.h \
	gcr/gcr-library.h \
	gcr/gcr-mock-prompter.h \
	gcr/gcr-parser.h \
	gcr/gcr-pkcs11-certificate.h \
	gcr/gcr-prompt.h \
	gcr/gcr-secret-exchange.h \
	gcr/gcr-secure-memory.h \
	gcr/gcr-simple-certificate.h \
	gcr/gcr-simple-collection.h \
	gcr/gcr-ssh-askpass.h \
	gcr/gcr-system-prompt.h \
	gcr/gcr-system-prompter.h \
	gcr/gcr-trust.h \
	gcr/gcr-types.h \
	gcr/gcr-union-collection.h \
	gcr/gcr-unlock-options.h \
	gcr/gcr-version.h

gcr_inc_HEADERS = \
	$(gcr_HEADER_FILES) \
	gcr/gcr-enum-types-base.h

if WITH_GTK
gcr_inc_HEADERS += gcr/gcr.h
endif

lib_LTLIBRARIES += \
	libgcr-base-@GCR_MAJOR@.la

gcr_BUILT_SOURCES = \
	gcr/gcr-marshal.c gcr/gcr-marshal.h \
	gcr/gcr-enum-types-base.c gcr/gcr-enum-types-base.h \
	gcr/gcr-oids.c gcr/gcr-oids.h \
	gcr/gcr-dbus-generated.c gcr/gcr-dbus-generated.h

BUILT_SOURCES += $(gcr_BUILT_SOURCES)

gcr_PUBLIC_FILES = \
	gcr/gcr-base.h \
	gcr/gcr-certificate.c gcr/gcr-certificate.h \
	gcr/gcr-certificate-chain.c gcr/gcr-certificate-chain.h \
	gcr/gcr-certificate-request.c gcr/gcr-certificate-request.h \
	gcr/gcr-collection.c gcr/gcr-collection.h \
	gcr/gcr-column.h \
	gcr/gcr-comparable.c gcr/gcr-comparable.h \
	gcr/gcr-filter-collection.c gcr/gcr-filter-collection.h \
	gcr/gcr-fingerprint.c gcr/gcr-fingerprint.h \
	gcr/gcr-icons.c gcr/gcr-icons.h \
	gcr/gcr-importer.c gcr/gcr-importer.h \
	gcr/gcr-import-interaction.c gcr/gcr-import-interaction.h \
	gcr/gcr-library.c gcr/gcr-library.h \
	gcr/gcr-mock-prompter.c gcr/gcr-mock-prompter.h \
	gcr/gcr-parser.c gcr/gcr-parser.h \
	gcr/gcr-pkcs11-certificate.c gcr/gcr-pkcs11-certificate.h \
	gcr/gcr-prompt.c gcr/gcr-prompt.h \
	gcr/gcr-secret-exchange.c gcr/gcr-secret-exchange.h \
	gcr/gcr-secure-memory.c gcr/gcr-secure-memory.h \
	gcr/gcr-simple-certificate.c gcr/gcr-simple-certificate.h \
	gcr/gcr-simple-collection.c gcr/gcr-simple-collection.h \
	gcr/gcr-ssh-askpass.c gcr/gcr-ssh-askpass.h \
	gcr/gcr-system-prompt.c gcr/gcr-system-prompt.h \
	gcr/gcr-system-prompter.c gcr/gcr-system-prompter.h \
	gcr/gcr-trust.c gcr/gcr-trust.h \
	gcr/gcr-types.h \
	gcr/gcr-union-collection.c gcr/gcr-union-collection.h \
	gcr/gcr-unlock-options.h \
	gcr/gcr-version.h \
	$(NULL)

gcr_PRIVATE_FILES = \
	gcr/gcr-callback-output-stream.c gcr/gcr-callback-output-stream.h \
	gcr/gcr-certificate-extensions.c gcr/gcr-certificate-extensions.h \
	gcr/gcr-column.c \
	gcr/gcr-dbus-constants.h \
	gcr/gcr-deprecated-base.h \
	gcr/gcr-gnupg-collection.c gcr/gcr-gnupg-collection.h \
	gcr/gcr-gnupg-importer.c gcr/gcr-gnupg-importer.h \
	gcr/gcr-gnupg-key.c gcr/gcr-gnupg-key.h \
	gcr/gcr-gnupg-process.c gcr/gcr-gnupg-process.h \
	gcr/gcr-gnupg-records.c gcr/gcr-gnupg-records.h \
	gcr/gcr-gnupg-util.c gcr/gcr-gnupg-util.h \
	gcr/gcr-internal.h \
	gcr/gcr-key-mechanisms.h gcr/gcr-key-mechanisms.c \
	gcr/gcr-memory-icon.c gcr/gcr-memory-icon.h \
	gcr/gcr-openpgp.c gcr/gcr-openpgp.h \
	gcr/gcr-openssh.c gcr/gcr-openssh.h \
	gcr/gcr-pkcs11-importer.c gcr/gcr-pkcs11-importer.h \
	gcr/gcr-record.c gcr/gcr-record.h \
	gcr/gcr-single-collection.c gcr/gcr-single-collection.h \
	gcr/gcr-subject-public-key.c gcr/gcr-subject-public-key.h \
	gcr/gcr-util.c gcr/gcr-util.h \
	$(NULL)

libgcr_base_@GCR_MAJOR@_la_SOURCES = \
	$(gcr_PUBLIC_FILES) \
	$(gcr_PRIVATE_FILES) \
	$(NULL)

nodist_libgcr_base_@GCR_MAJOR@_la_SOURCES = \
	$(gcr_BUILT_SOURCES)

libgcr_base_@GCR_MAJOR@_la_CFLAGS = \
	$(LIBGCRYPT_CFLAGS) \
	$(P11_KIT_CFLAGS) \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DP11_KIT_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION \
	-DG_LOG_DOMAIN=\"Gcr\"

libgcr_base_@GCR_MAJOR@_la_LDFLAGS = \
	-version-info $(GCR_LT_RELEASE) \
	-no-undefined \
	-export-symbols-regex '^_gcr_.*|^gcr_.*|^SECMEM_.*'

libgcr_base_@GCR_MAJOR@_la_LIBADD = \
	libegg.la \
	libgck-@GCK_MAJOR@.la \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS)

gcr/gcr-marshal.h: gcr/gcr-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) $(GLIB_GENMARSHAL) $< --header --prefix=_gcr_marshal > $@

gcr/gcr-marshal.c: gcr/gcr-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN) ( echo "#include \"gcr/gcr-marshal.h\"" > $@ && \
		$(GLIB_GENMARSHAL) $< --body --prefix=_gcr_marshal >> $@ )

gcr/gcr-enum-types-base.h: $(ENUM_TEMPLATE_H) $(gcr_HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@
gcr/gcr-enum-types-base.c: $(ENUM_TEMPLATE_C) $(gcr_HEADER_FILES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $^ > $@

gcr/gcr-oids.c: gcr/gcr-oids.list gcr/gcr-mkoids
	$(AM_V_GEN) sh $(srcdir)/gcr/gcr-mkoids -p GCR -c gcr/gcr-oids.c -h gcr/gcr-oids.h $(srcdir)/gcr/gcr-oids.list

gcr/gcr-oids.h: gcr/gcr-oids.c

DBUS_XML_DEFINITIONS = \
	gcr/org.gnome.keyring.Prompter.xml \
	gcr/org.gnome.keyring.Prompter2.xml

gcr/gcr-dbus-generated.c: $(DBUS_XML_DEFINITIONS)
	$(AM_V_GEN) $(GDBUS_CODEGEN) --interface-prefix org.gnome.keyring.internal. \
		--generate-c-code gcr/gcr-dbus-generated --c-namespace GcrDBus $^
	$(AM_V_GEN) sed -e 's/gcr_dbus/_gcr_dbus/g' \
		gcr/gcr-dbus-generated.c > gcr/gcr-dbus-generated.c.tmp && \
	mv gcr/gcr-dbus-generated.c.tmp gcr/gcr-dbus-generated.c
	$(AM_V_GEN) sed -e 's/gcr_dbus/_gcr_dbus/g' \
		gcr/gcr-dbus-generated.h > gcr/gcr-dbus-generated.h.tmp && \
	mv gcr/gcr-dbus-generated.h.tmp gcr/gcr-dbus-generated.h

gcr/gcr-dbus-generated.h: gcr/gcr-dbus-generated.c

libexec_PROGRAMS += gcr-ssh-askpass

gcr_ssh_askpass_SOURCES = gcr/gcr-ssh-askpass.c

gcr_ssh_askpass_CFLAGS = \
	-DGCR_SSH_ASKPASS_TOOL \
	-DGCR_COMPILATION \
	$(GLIB_CFLAGS)

gcr_ssh_askpass_LDADD = \
	libegg.la \
	$(GLIB_LIBS)

pkgconfig_DATA += \
	gcr-base-$(GCR_MAJOR).pc

gcr-base-$(GCR_MAJOR).pc: gcr/gcr-base.pc
	$(AM_V_GEN) cp gcr/gcr-base.pc gcr-base-$(GCR_MAJOR).pc

service_in_files = \
	gcr/org.gnome.keyring.SystemPrompter.service.in \
	gcr/org.gnome.keyring.PrivatePrompter.service.in
servicedir       = $(DBUS_SERVICES_DIR)
service_DATA     = $(service_in_files:.service.in=.service)

EXTRA_DIST += \
	gcr/gcr-base.pc.in \
	gcr/gcr-marshal.list \
	gcr/gcr-oids.list \
	gcr/gcr-mkoids \
	$(DBUS_XML_DEFINITIONS) \
	gcr/gcr-base.symbols \
	$(service_in_files) \
	gcr/gcr-version.h.in

CLEANFILES += \
	$(service_DATA) \
	gcr-base-actual.abi \
	gcr-base-expected.abi \
	Gcr-3.broken.gir

if HAVE_INTROSPECTION

INTROSPECTION_GIRS += Gcr-@GCR_MAJOR@.broken.gir

Gcr-@GCR_MAJOR@.gir: Gcr-@GCR_MAJOR@.broken.gir $(FIX_GIR)

Gcr-@GCR_MAJOR@.broken.gir: libgcr-base-@GCR_MAJOR@.la Gck-@GCK_MAJOR@.gir

Gcr_@GCR_MAJOR@_broken_gir_PACKAGES = p11-kit-1
Gcr_@GCR_MAJOR@_broken_gir_EXPORT_PACKAGES = gcr-base-@GCR_MAJOR@
Gcr_@GCR_MAJOR@_broken_gir_INCLUDES = GLib-2.0 GObject-2.0
Gcr_@GCR_MAJOR@_broken_gir_LIBS = libgcr-base-@GCR_MAJOR@.la
Gcr_@GCR_MAJOR@_broken_gir_CFLAGS = -I$(top_srcdir) -I$(top_builddir) \
	-DGCR_COMPILATION -DGCR_API_SUBJECT_TO_CHANGE
Gcr_@GCR_MAJOR@_broken_gir_FILES = $(gcr_PUBLIC_FILES)
Gcr_@GCR_MAJOR@_broken_gir_SCANNERFLAGS = \
	--add-include-path=$(top_builddir)/gck \
	--include-uninstalled=$(builddir)/Gck-@GCK_MAJOR@.gir \
	--c-include "gcr/gcr-base.h" \
	--identifier-prefix Gcr \
	--nsversion @GCR_MAJOR@

gir_DATA += Gcr-@GCR_MAJOR@.gir

if ENABLE_VAPIGEN

gcr-@GCR_MAJOR@.vapi: Gcr-@GCR_MAJOR@.gir gcr/Gcr-@GCR_MAJOR@.metadata gcr-@GCR_MAJOR@.deps gck-@GCK_MAJOR@.vapi

VAPIGEN_VAPIS += gcr-@GCR_MAJOR@.vapi

gcr_@GCR_MAJOR@_vapi_DEPS = glib-2.0 gio-2.0 gck-@GCK_MAJOR@
gcr_@GCR_MAJOR@_vapi_METADATADIRS = $(srcdir)/gcr
gcr_@GCR_MAJOR@_vapi_VAPIDIRS = $(builddir)
gcr_@GCR_MAJOR@_vapi_GIRDIRS = $(builddir)
gcr_@GCR_MAJOR@_vapi_FILES = Gcr-@GCR_MAJOR@.gir

gcr-$(GCR_MAJOR).deps: Makefile.am
	$(AM_V_GEN) echo $(gcr_@GCR_MAJOR@_vapi_DEPS) | tr ' ' '\n' > $@

EXTRA_DIST += \
	gcr/Gcr-@GCR_MAJOR@.metadata

endif # ENABLE_VAPIGEN

endif # HAVE_INTROSPECTON

# ----------------------------------------------------------------

gcr-base-expected.abi: gcr/gcr-base.symbols
	$(AM_V_GEN) cpp -P $< | sort > $@

gcr-base-actual.abi: $(builddir)/.libs/libgcr-base-@GCR_MAJOR@.so
	$(AM_V_GEN)  $(NM) -D -g --defined-only $< | \
		cut -d ' ' -f 3 | grep -Ev '^(__bss_start|_edata|_end|_gcr_)' | sort > $@

check-gcr-symbols: gcr-base-expected.abi gcr-base-actual.abi
	$(AM_V_GEN) diff -U0 --ignore-blank-lines gcr-base-expected.abi gcr-base-actual.abi || \
		(echo "check-symbols: Symbols have CHANGED. Fix gcr-base.symbols"; exit 1)

gcr_CFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DSRCDIR="\"@abs_srcdir@\"" \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION \
	$(GLIB_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(P11_KIT_CFLAGS)

gcr_LIBS = \
	libgcr-base-@GCR_MAJOR@.la \
	libegg.la \
	libgck-testable.la \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS)

gcr_TESTS = \
	test-util \
	test-filter-collection \
	test-secret-exchange \
	test-simple-certificate \
	test-certificate \
	test-certificate-chain \
	test-subject-public-key \
	test-fingerprint \
	test-pkcs11-certificate \
	test-openpgp \
	test-openssh \
	test-secure-memory \
	test-trust \
	test-parser \
	test-record \
	test-memory-icon \
	test-gnupg-key \
	test-gnupg-collection \
	test-gnupg-process \
	test-system-prompt \
	test-ssh-askpass

test_certificate_SOURCES = gcr/test-certificate.c
test_certificate_CFLAGS = $(gcr_CFLAGS)
test_certificate_LDADD = $(gcr_LIBS)

test_certificate_chain_SOURCES = gcr/test-certificate-chain.c
test_certificate_chain_CFLAGS = $(gcr_CFLAGS)
test_certificate_chain_LDADD = $(gcr_LIBS)

test_filter_collection_SOURCES = gcr/test-filter-collection.c
test_filter_collection_CFLAGS = $(gcr_CFLAGS)
test_filter_collection_LDADD = $(gcr_LIBS)

test_fingerprint_SOURCES = gcr/test-fingerprint.c
test_fingerprint_CFLAGS = $(gcr_CFLAGS)
test_fingerprint_LDADD = $(gcr_LIBS)

test_gnupg_collection_SOURCES = gcr/test-gnupg-collection.c
test_gnupg_collection_CFLAGS = $(gcr_CFLAGS)
test_gnupg_collection_LDADD = $(gcr_LIBS)

test_gnupg_key_SOURCES = gcr/test-gnupg-key.c
test_gnupg_key_CFLAGS = $(gcr_CFLAGS)
test_gnupg_key_LDADD = $(gcr_LIBS)

test_gnupg_process_SOURCES = gcr/test-gnupg-process.c
test_gnupg_process_CFLAGS = $(gcr_CFLAGS)
test_gnupg_process_LDADD = $(gcr_LIBS)

test_memory_icon_SOURCES = gcr/test-memory-icon.c
test_memory_icon_CFLAGS = $(gcr_CFLAGS)
test_memory_icon_LDADD = $(gcr_LIBS)

test_openpgp_SOURCES = gcr/test-openpgp.c
test_openpgp_CFLAGS = $(gcr_CFLAGS)
test_openpgp_LDADD = $(gcr_LIBS)

test_openssh_SOURCES = gcr/test-openssh.c
test_openssh_CFLAGS = $(gcr_CFLAGS)
test_openssh_LDADD = $(gcr_LIBS)

test_parser_SOURCES = gcr/test-parser.c
test_parser_CFLAGS = $(gcr_CFLAGS)
test_parser_LDADD = $(gcr_LIBS)

test_pkcs11_certificate_SOURCES = gcr/test-pkcs11-certificate.c
test_pkcs11_certificate_CFLAGS = $(gcr_CFLAGS)
test_pkcs11_certificate_LDADD = $(gcr_LIBS)

test_record_SOURCES = gcr/test-record.c
test_record_CFLAGS = $(gcr_CFLAGS)
test_record_LDADD = $(gcr_LIBS)

test_secret_exchange_SOURCES = gcr/test-secret-exchange.c
test_secret_exchange_CFLAGS = $(gcr_CFLAGS)
test_secret_exchange_LDADD = $(gcr_LIBS)

test_secure_memory_SOURCES = gcr/test-secure-memory.c
test_secure_memory_CFLAGS = $(gcr_CFLAGS)
test_secure_memory_LDADD = $(gcr_LIBS)

test_simple_certificate_SOURCES = gcr/test-simple-certificate.c
test_simple_certificate_CFLAGS = $(gcr_CFLAGS)
test_simple_certificate_LDADD = $(gcr_LIBS)

test_ssh_askpass_SOURCES = gcr/test-ssh-askpass.c
test_ssh_askpass_CFLAGS = $(gcr_CFLAGS)
test_ssh_askpass_LDADD = libegg-test.la $(gcr_LIBS)

test_subject_public_key_SOURCES = gcr/test-subject-public-key.c
test_subject_public_key_CFLAGS = $(gcr_CFLAGS)
test_subject_public_key_LDADD = $(gcr_LIBS)

test_system_prompt_SOURCES = gcr/test-system-prompt.c
test_system_prompt_CFLAGS = $(gcr_CFLAGS)
test_system_prompt_LDADD = $(gcr_LIBS)

test_trust_SOURCES = gcr/test-trust.c
test_trust_CFLAGS = $(gcr_CFLAGS)
test_trust_LDADD = $(gcr_LIBS)

test_util_SOURCES = gcr/test-util.c
test_util_CFLAGS = $(gcr_CFLAGS)
test_util_LDADD = $(gcr_LIBS)

check_PROGRAMS += $(gcr_TESTS)
TESTS += $(gcr_TESTS)

EXTRA_DIST += \
	gcr/fixtures

# ------------------------------------------------------------------

noinst_PROGRAMS += \
	frob-openpgp \
	frob-certificate-request \
	frob-parser

frob_certificate_request_SOURCES = \
	gcr/frob-certificate-request.c \
	gcr/console-interaction.c gcr/console-interaction.h
frob_certificate_request_CFLAGS = $(gcr_CFLAGS)
frob_certificate_request_LDADD = $(gcr_LIBS)

frob_openpgp_SOURCES = gcr/frob-openpgp.c
frob_openpgp_CFLAGS = $(gcr_CFLAGS)
frob_openpgp_LDADD = $(gcr_LIBS)

frob_parser_SOURCES = gcr/frob-parser.c
frob_parser_CFLAGS = $(gcr_CFLAGS)
frob_parser_LDADD = $(gcr_LIBS)
