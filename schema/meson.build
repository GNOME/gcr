compiled_schemas = gnome.compile_schemas()
install_data('org.gnome.crypto.pgp.gschema.xml',
  install_dir: gcr_prefix / get_option('datadir') / 'glib-2.0' / 'schemas',
)

# GConf files (legacy)
gconf_convert_files = [
  'org.gnome.crypto.pgp.convert',
  'org.gnome.crypto.pgp_keyservers.convert',
]
install_data(gconf_convert_files,
  install_dir: get_option('datadir') / 'GConf' / 'gsettings',
)
