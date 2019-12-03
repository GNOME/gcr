project('gcr', 'c',
  version: '3.38.0',
  meson_version: '>= 0.49',
  license: 'GPL2+',
)

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')

# Versioning
gcr_version = meson.project_version()
gcr_soversion = '1.0.0'
gcr_version_array = gcr_version.split('.')
gcr_major_version = gcr_version_array[0].to_int()
gcr_minor_version = gcr_version_array[1].to_int()
gcr_micro_version = gcr_version_array[2].to_int()

gck_version = '1.0.0'
gck_soversion = '0.0.0'
gck_version_array = gck_version.split('.')
gck_major_version = gck_version_array[0].to_int()
gck_minor_version = gck_version_array[1].to_int()
gck_micro_version = gck_version_array[2].to_int()

# Some variables
source_root = meson.current_source_dir()
build_root = meson.current_build_dir()
cc = meson.get_compiler('c')

gcr_prefix = get_option('prefix')
libexecbindir = gcr_prefix / get_option('libexecdir')
podir = source_root / 'po'

# Dependencies
min_glib_version = '2.44'
glib_dep = dependency('glib-2.0', version: '>=' + min_glib_version)
gmodule_dep = dependency('gmodule-no-export-2.0', version: '>=' + min_glib_version)
gthread_dep = dependency('gthread-2.0', version: '>=' + min_glib_version)
gobject_dep = dependency('gobject-2.0', version: '>=' + min_glib_version)
gio_dep = dependency('gio-2.0', version: '>=' + min_glib_version)
gio_unix_dep = dependency('gio-unix-2.0',version: '>=' + min_glib_version)
glib_deps = [ glib_dep, gmodule_dep, gthread_dep, gobject_dep, gio_dep, gio_unix_dep, ]
gpg_path = get_option('gpg_path')
if gpg_path == ''
  gpg_path = find_program('gpg2', 'gpg').path()
endif
libgcrypt_dep = dependency('libgcrypt', version: '>= 1')
p11kit_dep = dependency('p11-kit-1', version: '>= 0.19.0')
p11_system_config_modules = p11kit_dep.get_pkgconfig_variable('p11_system_config_modules')
if p11_system_config_modules == ''
  error('Couldn\'t find location for pkcs11 module config')
endif

if get_option('gtk')
  gtk_min_version = '3.22'
  gtk_dep = dependency('gtk+-3.0', version: '>=' + gtk_min_version)
endif

# configuration
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('LIBEXECDIR', libexecbindir)
conf.set_quoted('LOCALEDIR', gcr_prefix / get_option('localedir'))
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set('HAVE_GETTEXT', true)
conf.set('HAVE_LOCALE_H', cc.has_header('locale.h'))
conf.set('HAVE_TIMEGM', cc.has_function('timegm'))
conf.set('HAVE_MLOCK', cc.has_function('mlock'))
conf.set_quoted('GPG_EXECUTABLE', gpg_path)
conf.set_quoted('LIBGCRYPT_VERSION', libgcrypt_dep.version())
config_file = configure_file(
  output: 'config.h',
  configuration: conf,
)
config_h_dir = include_directories('.')

# subdirs
subdir('po')
subdir('egg')
subdir('gck')
subdir('gcr')
subdir('schema')
if get_option('gtk')
  subdir('ui')
endif
if get_option('gtk_doc')
  subdir('docs')
endif

# Post-install scripts
meson.add_install_script('meson_post_install.py',
  get_option('datadir'),
  get_option('libdir'),
  gcr_major_version.to_string(),
  gcr_soversion,
)
