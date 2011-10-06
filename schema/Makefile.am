
gsettings_SCHEMAS = \
	org.gnome.crypto.pgp.gschema.xml

@GSETTINGS_RULES@

convert_DATA = \
	org.gnome.crypto.pgp.convert \
	org.gnome.crypto.pgp_keyservers.convert

convertdir = \
	$(datarootdir)/GConf/gsettings

EXTRA_DIST = \
	$(convert_DATA) \
	$(gsettings_SCHEMAS)