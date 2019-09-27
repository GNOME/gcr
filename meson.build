project('gcr', 'c',
  version: '3.28.0',
  meson_version: '>= 0.49',
  license: 'GPL2+',
)

gnome = import('gnome')
i18n = import('i18n')

# Versioning
gcr_version = meson.project_version()
gcr_soversion = '1.0.0'
gcr_major_version = gcr_version.split('.')[0]
gcr_minor_version = gcr_version.split('.')[1]
gcr_micro_version = gcr_version.split('.')[2]

gck_version = '1.0.0'
gck_soversion = '0.0.0'
gck_major_version = gck_version.split('.')[0]
gck_minor_version = gck_version.split('.')[1]
gck_micro_version = gck_version.split('.')[2]

# Options
gtk_enabled = get_option('gtk')

# Some variables
cc = meson.get_compiler('c')
buildtype = get_option('buildtype')

gcr_prefix = get_option('prefix')
datadir = gcr_prefix / get_option('datadir')
bindir = gcr_prefix / get_option('bindir')
libdir = gcr_prefix / get_option('libdir')
libexecbindir = gcr_prefix / get_option('libexecdir') / meson.project_name()
pkgdatadir = datadir / meson.project_name()

# Dependencies
min_glib_version = '2.38'
glib_deps = [
  dependency('glib-2.0', version: '>=' + min_glib_version),
  dependency('gmodule-no-export-2.0', version: '>=' + min_glib_version),
  dependency('gthread-2.0', version: '>=' + min_glib_version),
  dependency('gobject-2.0', version: '>=' + min_glib_version),
  dependency('gio-2.0', version: '>=' + min_glib_version),
  dependency('gio-unix-2.0',version: '>=' + min_glib_version),
]
gpg_bin = find_program('gpg2', 'gpg')
libgcrypt_dep = dependency('libgcrypt', version: '>= 1')
p11kit_dep = dependency('p11-kit-1', version: '>+ 0.19.0')
p11_system_config_modules = p11kit_dep.get_pkgconfig_variable('p11_system_config_modules')
if p11_system_config_modules == 'Could not find location for pkcs11 module config'
  error('Could not find location for pkcs11 module config')
endif

if gtk_enabled
  gtk_min_version = '3.12'
  gtk_dep = dependency('gtk+-3.0', version: '>=' + gtk_min_version)
  gtk_x11_dep = dependency('gtk+-x11-3.0', version: '>=' + gtk_min_version)
endif

# configuration
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), meson.project_version()))
conf.set_quoted('SRCDIR', meson.source_root())
conf.set_quoted('BUILDDIR', meson.build_root())
conf.set_quoted('PKGDATADIR', pkgdatadir)
conf.set_quoted('LIBEXECDIR', libexecbindir)
conf.set_quoted('LOCALEDIR', gcr_prefix / get_option('localedir'))
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
conf.set('WITH_DEBUG', buildtype.contains('debug'))
conf.set10('_DEBUG', buildtype.contains('debug'))
conf.set('HAVE_TIMEGM', cc.has_function('timegm'))
conf.set('HAVE_MLOCK', cc.has_function('mlock'))
conf.set_quoted('GPG_EXECUTABLE', gpg_bin.path())
conf.set_quoted('LIBGCRYPT_VERSION', libgcrypt_dep.version())
config_file = configure_file(
  output: 'config.h',
  configuration: conf,
)
config_h_dir = include_directories('.')

# Post-install scripts
meson.add_install_script('meson_post_install.py')

# subdirs
subdir('po')
subdir('egg')
subdir('gck')
subdir('gcr')
subdir('schema')
if gtk_enabled
  subdir('ui')
endif
