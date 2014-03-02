
SUBDIRS += \
	ui/icons/16x16 \
	ui/icons/22x22 \
	ui/icons/24x24 \
	ui/icons/32x32 \
	ui/icons/48x48 \
	ui/icons/256x256

EXTRA_DIST += \
	ui/icons/src \
	ui/icons/render-icons.py

if WITH_UPDATE_ICON_CACHE

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi
endif

render:
	python ui/icons/render-icons.py
