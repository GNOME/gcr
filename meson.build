project('gcr', 'c',
  version: '4.3.0',
  meson_version: '>= 0.59',
  license: 'GPL2+',
)

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')

# Versioning
gcr_api_version = '4'
gcr_version = meson.project_version()
gcr_soversion = '4'
gcr_version_array = gcr_version.split('.')
gcr_major_version = gcr_version_array[0].to_int()
gcr_minor_version = gcr_version_array[1].to_int()
gcr_micro_version = gcr_version_array[2].to_int()

gck_api_version = '2'
gck_version = '2.3.0'
gck_soversion = '2'
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

gck_basename = 'gck-@0@'.format(gck_api_version)
gcr_base_basename = 'gcr-base-@0@'.format(gcr_api_version)
gcr_basename = 'gcr-@0@'.format(gcr_api_version)

# Dependencies
min_glib_version = '2.68'
glib_dep = dependency('glib-2.0', version: '>=' + min_glib_version)
gmodule_dep = dependency('gmodule-no-export-2.0', version: '>=' + min_glib_version)
gthread_dep = dependency('gthread-2.0', version: '>=' + min_glib_version)
gobject_dep = dependency('gobject-2.0', version: '>=' + min_glib_version)
gio_dep = dependency('gio-2.0', version: '>=' + min_glib_version)
gio_unix_dep = dependency('gio-unix-2.0',version: '>=' + min_glib_version)
glib_deps = [ glib_dep, gmodule_dep, gthread_dep, gobject_dep, gio_dep, gio_unix_dep, ]
gpg_path = get_option('gpg_path')
if gpg_path == ''
  gpg_path = find_program('gpg2', 'gpg').full_path()
endif

with_gcrypt = false
with_gnutls = false

crypto_deps = []

if get_option('crypto') == 'libgcrypt'
  min_libgcrypt_version = '1.2.2'
  libgcrypt_dep = dependency(
    'libgcrypt',
    version: '>=' + min_libgcrypt_version,
  )
  with_gcrypt = true
  crypto_deps += libgcrypt_dep
elif get_option('crypto') == 'gnutls'
  min_gnutls_version = '3.8.5'
  gnutls_dep = dependency(
    'gnutls',
    version: '>=' + min_gnutls_version,
  )
  with_gnutls = true
  crypto_deps += gnutls_dep
endif

p11kit_dep = dependency('p11-kit-1', version: '>= 0.19.0')
p11_system_config_modules = p11kit_dep.get_variable('p11_system_config_modules')
if p11_system_config_modules == ''
  error('Couldn\'t find location for pkcs11 module config')
endif
libsecret_dep = dependency('libsecret-1', version: '>= 0.20', required: get_option('ssh_agent'))
if get_option('ssh_agent')
  ssh_add_path = find_program('ssh-add').full_path()
  ssh_agent_path = find_program('ssh-agent').full_path()
endif

with_systemd = false
libsystemd_deps = []
libsystemd = dependency('libsystemd', required: get_option('systemd'))
systemd = dependency('systemd', required: get_option('systemd'))
if libsystemd.found() and systemd.found()
  systemduserunitdir = systemd.get_variable('systemduserunitdir',
    pkgconfig_define: [ 'prefix', get_option('prefix') ],
    default_value: 'lib' / 'systemd' / 'user',
  )
  libsystemd_deps += libsystemd
  with_systemd = true
endif

if get_option('gtk4')
  gtk4_dep = dependency('gtk4')
endif

enable_gir = get_option('introspection')
enable_vapi = get_option('vapi')
if enable_vapi
  assert(enable_gir, 'vapi support was requested, but introspection support is mandatory.')
endif

# configuration
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('LIBEXECDIR', libexecbindir)
conf.set_quoted('LOCALEDIR', gcr_prefix / get_option('localedir'))
conf.set_quoted('GETTEXT_PACKAGE', gcr_basename)
conf.set('HAVE_GETTEXT', true)
conf.set('HAVE_LOCALE_H', cc.has_header('locale.h'))
conf.set('HAVE_TIMEGM', cc.has_function('timegm'))
conf.set('HAVE_MLOCK', cc.has_function('mlock'))
conf.set_quoted('GPG_EXECUTABLE', gpg_path)
if with_gcrypt
  conf.set_quoted('LIBGCRYPT_VERSION', libgcrypt_dep.version())
endif
conf.set('WITH_GCRYPT', with_gcrypt)
conf.set('WITH_GNUTLS', with_gnutls)
if get_option('ssh_agent')
  conf.set_quoted('SSH_ADD_EXECUTABLE', ssh_add_path)
  conf.set_quoted('SSH_AGENT_EXECUTABLE', ssh_agent_path)
endif
conf.set10('WITH_SYSTEMD', with_systemd)
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
subdir('tools')
if get_option('gtk_doc')
  if not get_option('introspection')
    error('Can\'t generate docs without introspection enabled!')
  endif

  subdir('docs')
endif
