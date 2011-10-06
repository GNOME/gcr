SUBDIRS = \
	. \
	egg \
	gck \
	gcr \
	schema \
	testing \
	po \
	docs

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	HACKING

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-gtk-doc \
	--enable-strict \
	--disable-coverage \
	--disable-update-mime \
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

if WITH_COVERAGE
coverage:
	mkdir -p testing/coverage
	$(LCOV) --directory docs/ --zerocounters
	$(LCOV) --directory . --capture --output-file testing/coverage.info
	$(GENHTML) --output-directory testing/coverage testing/coverage.info
	$(LCOV) --directory . --zerocounters
	@echo "file://$(abs_top_builddir)/testing/coverage/index.html"

clear-coverage:
	$(LCOV) --directory . --zerocounters

endif
