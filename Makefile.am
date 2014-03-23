NULL =

SUBDIRS = \
	. \
	po \
	docs/reference/gck \
	docs/reference/gcr

ACLOCAL_AMFLAGS = -I build/m4 ${ACLOCAL_FLAGS}

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-strict \
	--disable-coverage \
	--disable-update-mime \
	--disable-update-icon-cache \
	--with-gtk \
	--with-pkcs11-modules=$(abs_srcdir)/$(top_distdir)/_inst/lib/

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		( cd "$(top_srcdir)" && \
		  echo '# Generate automatically. Do not edit.'; echo; \
		  $(top_srcdir)/missing --run git log --stat --date=short ) > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		     echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi

check-local: check-gck-symbols check-gcr-symbols check-ui-symbols

upload-release: $(DIST_ARCHIVES)
	scp $(DIST_ARCHIVES) master.gnome.org:
	ssh master.gnome.org ftpadmin install $(DIST_ARCHIVES)

# Default executable tests
LOG_DRIVER = $(srcdir)/build/tap-driver
LOG_DRIVER_FLAGS = --format=tap
LOG_COMPILER = $(srcdir)/build/tap-gtester

VALGRIND_ARGS = --trace-children=no --quiet --error-exitcode=33 \
	--suppressions=valgrind-suppressions --gen-suppressions=all \
	--num-callers=16
VALGRIND_SUPPRESSIONS = \
	build/gcr.supp \
	build/gcrypt.supp \
	build/glib.supp \
	build/glibc.supp \
	build/p11-kit.supp \
	build/pixman.supp \
	build/pthread.supp \
	build/unknown.supp

valgrind-suppressions: $(VALGRIND_SUPPRESSIONS)
	$(AM_V_GEN) cat $^ > $@

check-memory: valgrind-suppressions
	$(MAKE) LOG_FLAGS="-- libtool --mode=execute valgrind $(VALGRIND_ARGS)" \
		$(AM_MAKEFLAGS) check
recheck-memory: valgrind-suppressions
	$(MAKE) LOG_FLAGS="-- libtool --mode=execute valgrind $(VALGRIND_ARGS)" \
		$(AM_MAKEFLAGS) recheck

AM_CPPFLAGS = \
	-I$(srcdir) \
	-I$(builddir) \
	-I$(srcdir)/build \
	-DSRCDIR="\"@abs_srcdir@\"" \
	-DBUILDDIR="\"@abs_builddir@\"" \
	-DLOCALEDIR=\""$(datadir)/locale"\" \
	$(GLIB_CFLAGS)

LDADD = \
	$(GLIB_LIBS)

BUILT_SOURCES =
TESTS =

check_PROGRAMS =
check_LTLIBRARIES =
lib_LTLIBRARIES =
noinst_LTLIBRARIES =
noinst_PROGRAMS = $(check_PROGRAMS)

pkgconfig_DATA =
pkgconfigdir = $(libdir)/pkgconfig

CLEANFILES = \
	$(pkgconfig_DATA)

EXTRA_DIST = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	HACKING \
	build/valgrind \
	$(VALGRIND_SUPPRESSIONS) \
	build/enum-template.c \
	build/enum-template.h \
	build/g-ir-unbreak.xsl \
	build/tap-driver \
	build/tap-gtester \
	build/gdbus-unbreak-codegen \
	$(NULL)

TEST_SUPPRESSIONS = $(top_builddir)/build/valgrind-suppressions

GDBUS_CODEGEN = $(top_srcdir)/build/gdbus-unbreak-codegen

V_SED = $(V_SED_$(V))
V_SED_ = $(V_SED_$(AM_DEFAULT_VERBOSITY))
V_SED_0 = @echo "  SED     " $@;

SED_SUBST = sed \
	-e 's,[@]datadir[@],$(datadir),g' \
	-e 's,[@]libexecdir[@],$(libexecdir),g' \
	-e 's,[@]libdir[@],$(libdir),g' \
	-e 's,[@]includedir[@],$(includedir),g' \
	-e 's,[@]datarootdir[@],$(datarootdir),g' \
	-e 's,[@]sysconfdir[@],$(sysconfdir),g' \
	-e 's,[@]bindir[@],$(bindir),g' \
	-e 's,[@]exec_prefix[@],$(exec_prefix),g' \
	-e 's,[@]prefix[@],$(prefix),g' \
	-e 's,[@]PACKAGE[@],$(PACKAGE),g' \
	-e 's,[@]VERSION[@],$(VERSION),g' \
	$(NULL)

.desktop.in.in.desktop.in:
	$(V_SED) $(SED_SUBST) $< > $@

@INTLTOOL_DESKTOP_RULE@

.service.in.service:
	$(V_SED) $(SED_SUBST) $< > $@

ENUM_TEMPLATE_C = $(top_srcdir)/build/enum-template.c
ENUM_TEMPLATE_H = $(top_srcdir)/build/enum-template.h

V_XSLTPROC = $(V_XSLTPROC_$(V))
V_XSLTPROC_ = $(V_XSLTPROC_$(AM_DEFAULT_VERBOSITY))
V_XSLTPROC_0 = @echo "  XSLTPROC" $@;

FIX_GIR = $(top_srcdir)/build/g-ir-unbreak.xsl

.broken.gir.gir:
	$(V_XSLTPROC) xsltproc -o $@ $(FIX_GIR) $<
.broken.typelib.typelib:
	$(AM_V_GEN) cp $< $@

SUFFIXES = .desktop.in .desktop.in.in .service .service.in .broken.typelib .broken.gir .gir

if HAVE_INTROSPECTION

include $(INTROSPECTION_MAKEFILE)

INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = $(INTROSPECTION_FLAGS) --warn-all --add-include-path=.
INTROSPECTION_COMPILER_ARGS = --includedir=.

girdir = $(datadir)/gir-1.0
gir_DATA =

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(gir_DATA:.gir=.typelib)

CLEANFILES += \
	$(gir_DATA) \
	$(typelibs_DATA) \
	$(BUILT_SOURCES)

if ENABLE_VAPIGEN

include $(VAPIGEN_MAKEFILE)

VAPIGEN_VAPIS =
VAPI_DEPS = $(VAPIGEN_VAPIS:.vapi=.deps)

vapidir = $(datadir)/vala/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPI_DEPS)

CLEANFILES += \
	$(VAPIGEN_VAPIS) \
	$(VAPI_DEPS)

endif

endif

include egg/Makefile.am
include gck/Makefile.am
include gcr/Makefile.am
include schema/Makefile.am

if WITH_GTK
include ui/Makefile.am
include ui/icons/Makefile.am
endif
