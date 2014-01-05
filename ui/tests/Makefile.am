include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-DSRCDIR="\"@abs_srcdir@\"" \
	-DGCR_API_SUBJECT_TO_CHANGE \
	-DGCK_API_SUBJECT_TO_CHANGE \
	-DGCR_COMPILATION \
	$(GLIB_CFLAGS) \
	$(LIBGCRYPT_CFLAGS) \
	$(P11_KIT_CFLAGS) \
	$(GTK_CFLAGS)

LDADD = \
	$(top_builddir)/ui/libgcr-ui-@GCR_MAJOR@.la \
	$(top_builddir)/gcr/libgcr-base-@GCR_MAJOR@.la \
	$(top_builddir)/egg/libegg.la \
	$(top_builddir)/gck/libgck-testable.la \
	$(GLIB_LIBS) \
	$(LIBGCRYPT_LIBS) \
	$(P11_KIT_LIBS) \
	$(GTK_LIBS)

# ------------------------------------------------------------------

noinst_PROGRAMS = \
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

frob_unlock_SOURCES = \
	frob-unlock.c \
	../gcr-viewer-window.c
