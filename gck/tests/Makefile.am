include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DSRCDIR="\"@abs_srcdir@\"" \
	-DBUILDDIR="\"@abs_builddir@\"" \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DG_LOG_DOMAIN=\"Gck\" \
	$(P11_KIT_CFLAGS) \
	$(GLIB_CFLAGS)

LDADD =  \
	$(top_builddir)/gck/libgck-testable.la \
	$(top_builddir)/egg/libegg-test.la \
	$(top_builddir)/egg/libegg-hex.la \
	$(GTHREAD_LIBS) \
	$(GLIB_LIBS) \
	$(P11_KIT_LIBS) \
	$(GIO_LIBS)

TEST_PROGS = \
	test-gck-attributes \
	test-gck-module \
	test-gck-slot \
	test-gck-session \
	test-gck-object \
	test-gck-crypto \
	test-gck-uri \
	test-gck-enumerator \
	test-gck-modules

test_gck_enumerator_SOURCES = \
	test-gck-enumerator.c \
	mock-interaction.c mock-interaction.h

test_gck_session_SOURCES = \
	test-gck-session.c \
	mock-interaction.c mock-interaction.h

check_PROGRAMS = $(TEST_PROGS)

test: $(TEST_PROGS)
	gtester --verbose -m $(TEST_MODE) --g-fatal-warnings $(TEST_PROGS)

check-local: test

check-memory: perform-memcheck

all-local: $(check_PROGRAMS) $(check_LTLIBRARIES)

check_LTLIBRARIES = libmock-test-module.la

libmock_test_module_la_LDFLAGS = \
	-module -avoid-version -shared -rpath $(abs_builddir)

libmock_test_module_la_CFLAGS = \
	-I$(top_srcdir)/gck \
	-I$(top_srcdir) \
	$(GLIB_CFLAGS) \
	-DGCK_API_SUBJECT_TO_CHANGE

libmock_test_module_la_SOURCES = \
	mock-test-module.c

libmock_test_module_la_LIBADD = \
	$(top_builddir)/gck/libgck-testable.la
