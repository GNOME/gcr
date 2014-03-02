# included in top-level Makefile.am

VALGRIND_CONTRIB = \
	build/valgrind/valgrind.h \
	build/valgrind/memcheck.h

SUPPRESSIONS = \
	build/gcr.supp \
	build/gcrypt.supp \
	build/glib.supp \
	build/glibc.supp \
	build/p11-kit.supp \
	build/pixman.supp \
	build/pthread.supp \
	build/unknown.supp

valgrind-suppressions: $(SUPPRESSIONS)
	$(AM_V_GEN) sh -c "cd $(srcdir) && cat $(SUPPRESSIONS)" > $@

EXTRA_DIST += \
	$(VALGRIND_CONTRIB) \
	$(SUPPRESSIONS) \
	build/enum-template.c \
	build/enum-template.h \
	build/g-ir-unbreak.xsl \
	build/tap-driver \
	build/tap-compiler

all-local: valgrind-suppressions

CLEANFILES += \
	coverage \
	coverage.info \
	valgrind-suppressions
