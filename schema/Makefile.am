
gsettings_SCHEMAS = \
	schema/org.gnome.crypto.pgp.gschema.xml

@GSETTINGS_RULES@

convert_DATA = \
	schema/org.gnome.crypto.pgp.convert \
	schema/org.gnome.crypto.pgp_keyservers.convert

convertdir = \
	$(datarootdir)/GConf/gsettings

EXTRA_DIST += \
	$(convert_DATA) \
	$(gsettings_SCHEMAS) \
	$(gsettings_SCHEMAS:.xml=.valid) \
	$(NULL)
