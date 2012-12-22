
VALGRIND_CONTRIB = \
	valgrind/valgrind.h \
	valgrind/memcheck.h

SUPPRESSIONS = \
	gcr.supp \
	gcrypt.supp \
	glib.supp \
	glibc.supp \
	p11-kit.supp \
	pixman.supp \
	pthread.supp \
	unknown.supp

valgrind-suppressions: $(SUPPRESSIONS)
	$(AM_V_GEN) sh -c "cd $(srcdir) && cat $(SUPPRESSIONS)" > $@

EXTRA_DIST = \
	$(VALGRIND_CONTRIB) \
	$(SUPPRESSIONS) \
	enum-template.c \
	enum-template.h

all-local: valgrind-suppressions

CLEANFILES = \
	coverage \
	coverage.info \
	valgrind-suppressions
