include $(top_srcdir)/Makefile.decl

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DSRCDIR="\"@abs_srcdir@\"" \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION \
	$(GLIB_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(P11_KIT_CFLAGS)

LDADD = \
	$(top_builddir)/gcr/libgcr-testable.la \
	$(top_builddir)/egg/libegg.la \
	$(top_builddir)/gck/libgck-testable.la \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS)

TEST_PROGS = \
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
	test-system-prompt

if WITH_GTK

INCLUDES += \
	$(GTK_CFLAGS)
LDADD += \
	$(GTK_LIBS)

endif # WITH_GTK

check_PROGRAMS = $(TEST_PROGS)

test: $(TEST_PROGS)
	chmod -f 700 $(srcdir)/files/gnupg-homedir
	gtester --verbose -m $(TEST_MODE) --g-fatal-warnings $(TEST_PROGS)

check-local: test

check-memory: perform-memcheck

all-local: $(check_PROGRAMS)

EXTRA_DIST = \
	files

# ------------------------------------------------------------------

noinst_PROGRAMS = \
	frob-openpgp \
	frob-parser

if WITH_GTK

noinst_PROGRAMS += \
	frob-certificate \
	frob-certificate-request \
	frob-combo-selector \
	frob-gnupg-selector \
	frob-key \
	frob-tree-selector \
	frob-parser \
	frob-prompt \
	frob-request \
	frob-system-prompt \
	frob-unlock \
	frob-unlock-options

endif # WITH_GTK

frob_unlock_SOURCES = \
	frob-unlock.c \
	../gcr-viewer-window.c

frob_certificate_request_SOURCES = \
	frob-certificate-request.c \
	console-interaction.c console-interaction.h
