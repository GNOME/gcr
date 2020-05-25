gcr_base_headers_install_dir = 'gcr-@0@'.format(gcr_major_version) / 'gcr'

gcr_base_public_sources = files(
  'gcr-certificate.c',
  'gcr-certificate-chain.c',
  'gcr-certificate-request.c',
  'gcr-collection.c',
  'gcr-comparable.c',
  'gcr-filter-collection.c',
  'gcr-fingerprint.c',
  'gcr-icons.c',
  'gcr-importer.c',
  'gcr-import-interaction.c',
  'gcr-library.c',
  'gcr-mock-prompter.c',
  'gcr-parser.c',
  'gcr-pkcs11-certificate.c',
  'gcr-prompt.c',
  'gcr-secret-exchange.c',
  'gcr-secure-memory.c',
  'gcr-simple-certificate.c',
  'gcr-simple-collection.c',
  'gcr-ssh-askpass.c',
  'gcr-system-prompt.c',
  'gcr-system-prompter.c',
  'gcr-trust.c',
  'gcr-union-collection.c',
)

gcr_base_private_sources = files(
  'gcr-callback-output-stream.c',
  'gcr-certificate-extensions.c',
  'gcr-column.c',
  'gcr-gnupg-collection.c',
  'gcr-gnupg-importer.c',
  'gcr-gnupg-key.c',
  'gcr-gnupg-process.c',
  'gcr-gnupg-records.c',
  'gcr-gnupg-util.c',
  'gcr-key-mechanisms.c',
  'gcr-memory-icon.c',
  'gcr-openpgp.c',
  'gcr-openssh.c',
  'gcr-pkcs11-importer.c',
  'gcr-record.c',
  'gcr-single-collection.c',
  'gcr-subject-public-key.c',
  'gcr-util.c',
)

gcr_base_headers = files(
  'gcr-base.h',
  'gcr-certificate.h',
  'gcr-certificate-chain.h',
  'gcr-certificate-request.h',
  'gcr-collection.h',
  'gcr-column.h',
  'gcr-comparable.h',
  'gcr-deprecated-base.h',
  'gcr-fingerprint.h',
  'gcr-filter-collection.h',
  'gcr-icons.h',
  'gcr-importer.h',
  'gcr-import-interaction.h',
  'gcr-library.h',
  'gcr-mock-prompter.h',
  'gcr-parser.h',
  'gcr-pkcs11-certificate.h',
  'gcr-prompt.h',
  'gcr-secret-exchange.h',
  'gcr-secure-memory.h',
  'gcr-simple-certificate.h',
  'gcr-simple-collection.h',
  'gcr-ssh-askpass.h',
  'gcr-system-prompt.h',
  'gcr-system-prompter.h',
  'gcr-trust.h',
  'gcr-types.h',
  'gcr-union-collection.h',
  'gcr-unlock-options.h',
)

# Generated sources
gcr_version_conf = {
  'GCR_MAJOR': gcr_major_version,
  'GCR_MINOR': gcr_minor_version,
  'GCR_MICRO': gcr_micro_version,
}
gcr_version_h = configure_file(input: 'gcr-version.h.in',
  output: 'gcr-version.h',
  configuration: gcr_version_conf,
)

gcr_base_headers += gcr_version_h

gcr_enums_gen = gnome.mkenums_simple('gcr-enum-types-base',
  sources: gcr_base_headers,
  install_header: true,
  install_dir: get_option('includedir') / gcr_base_headers_install_dir,
)

gcr_marshal_gen = gnome.genmarshal('gcr-marshal',
  sources: 'gcr-marshal.list',
  prefix: '_gcr_marshal',
)

gcr_dbus_gen = gnome.gdbus_codegen('gcr-dbus-generated',
  sources: 'org.gnome.keyring.Prompter.xml',
  interface_prefix: 'org.gnome.keyring.internal.',
  namespace: '_GcrDbus',
)

gcr_oids = custom_target('gcr-oids',
  output: [
    'gcr-oids.c',
    'gcr-oids.h',
  ],
  input: 'gcr-oids.list',
  command: [ find_program('gcr-mkoids'),
    '-p', 'GCR',
    '-c', '@OUTPUT0@',
    '-h', '@OUTPUT1@',
    '@INPUT@',
  ]
)

gcr_base_sources = [
  gcr_base_private_sources,
  gcr_base_public_sources,
  gcr_enums_gen,
  gcr_marshal_gen,
  gcr_dbus_gen,
  gcr_oids,
]

gcr_base_deps = [
  glib_deps,
  p11kit_dep,
  libegg_dep,
  gck_dep,
]

gcr_base_cflags = [
  '-DG_LOG_DOMAIN="Gcr"',
  '-DGCR_COMPILATION',
  '-DGCR_API_SUBJECT_TO_CHANGE',
  '-DGCK_API_SUBJECT_TO_CHANGE',
  '-DP11_KIT_API_SUBJECT_TO_CHANGE',
  '-D_XOPEN_SOURCE',   # Needed for strptime()
]

gcr_base_symbolmap = meson.current_source_dir() / 'libgcr-base.map'
gcr_base_linkflags = cc.get_supported_link_arguments(
  '-Wl,--version-script,@0@'.format(gcr_base_symbolmap),
)

gcr_base_lib = shared_library('gcr-base-@0@'.format(gcr_major_version),
  gcr_base_sources,
  dependencies: gcr_base_deps,
  c_args: gcr_base_cflags,
  link_args: gcr_base_linkflags,
  link_depends: gcr_base_symbolmap,
  include_directories: config_h_dir,
  version: gcr_soversion,
  install: true,
)

install_headers(gcr_base_headers,
  subdir: gcr_base_headers_install_dir,
)

if get_option('gtk')
  install_headers('gcr.h',
    subdir: gcr_base_headers_install_dir,
  )
endif

gcr_base_dep = declare_dependency(
  link_with: gcr_base_lib,
  include_directories: include_directories('..'),
  sources: gcr_enums_gen[1], # Make sure gcr-enum-types-base.h can be included
)

if get_option('introspection')
  gcr_gir = gnome.generate_gir(gcr_base_lib,
    sources: [ gcr_base_public_sources, gcr_base_headers ],
    namespace: 'Gcr',
    nsversion: '@0@'.format(gcr_major_version),
    packages: gcr_base_deps,
    export_packages: 'gcr-base-@0@'.format(gcr_major_version),
    includes: [
      'GObject-2.0',
      'Gio-2.0',
      gck_gir[0],
    ],
    header: 'gcr/gcr-base.h',
    extra_args: [
      '-DGCR_COMPILATION',
      '-DGCR_API_SUBJECT_TO_CHANGE',
    ],
    install: true,
  )

  gcr_vapi = gnome.generate_vapi('gcr-@0@'.format(gcr_major_version),
    sources: gcr_gir[0],
    packages: [ 'glib-2.0', 'gio-2.0', gck_vapi ],
    metadata_dirs: meson.current_source_dir(),
    vapi_dirs: [
      build_root / 'gck',
    ],
    gir_dirs: [
      build_root / 'gck',
    ],
    install: true,
  )
endif

# pkg-config file
pkgconfig.generate(gcr_base_lib,
  subdirs: 'gcr-@0@'.format(gcr_major_version),
  requires: [ glib_dep, gio_dep, gobject_dep, p11kit_dep, 'gck-@0@'.format(gck_major_version) ],
  description: 'GObject and GUI library for high level crypto parsing and display',
)

# gcr-ssh-askpass binary
gcr_ssh_askpass = executable('gcr-ssh-askpass',
  'gcr-ssh-askpass.c',
  dependencies: [ glib_deps, libegg_dep ],
  c_args: [
    '-DGCR_SSH_ASKPASS_TOOL',
    '-DGCR_COMPILATION',
  ],
  include_directories: config_h_dir,
  install: true,
  install_dir: get_option('libexecdir'),
)

# Services
gcr_service_files = [
  'org.gnome.keyring.SystemPrompter.service',
  'org.gnome.keyring.PrivatePrompter.service',
]

foreach service_file : gcr_service_files
  configure_file(
    output: service_file,
    input: service_file + '.in',
    configuration: {
      'libexecdir': gcr_prefix / get_option('libexecdir'),
    },
    install_dir: get_option('datadir') / 'dbus-1' / 'services',
  )
endforeach

# Tests
gcr_test_names = [
  'util',
  'filter-collection',
  'secret-exchange',
  'simple-certificate',
  'certificate',
  'certificate-chain',
  'subject-public-key',
  'fingerprint',
  'pkcs11-certificate',
  'openpgp',
  'openssh',
  'secure-memory',
  'trust',
  'parser',
  'record',
  'memory-icon',
  'gnupg-key',
  'gnupg-collection',
  'gnupg-process',
  'system-prompt',
  'ssh-askpass',
]

gcr_test_cflags = [
  '-DSRCDIR="@0@"'.format(source_root),
  '-D_GCR_TEST_SSH_ASKPASS_PATH="@0@"'.format(gcr_ssh_askpass.full_path()),
]

foreach _test : gcr_test_names
  test_bin = executable('test-'+_test,
    'test-@0@.c'.format(_test),
    dependencies: [ gcr_base_deps, gcr_base_dep ],
    link_with: [ gck_test_lib, egg_test_lib ],
    c_args: [ gcr_base_cflags, gcr_test_cflags ],
    include_directories: config_h_dir,
  )

  test(_test, test_bin,
    suite: 'gcr-base',
    depends: gcr_ssh_askpass,
  )
endforeach

# Example frob programs
frob_certificate_request = executable('frob-certificate-request',
  files('frob-certificate-request.c', 'console-interaction.c'),
  dependencies: [ gcr_base_dep, gcr_base_deps ],
  c_args: gcr_base_cflags,
)

frob_openpgp = executable('frob-openpgp',
  files('frob-openpgp.c'),
  dependencies: [ gcr_base_dep, gcr_base_deps ],
  c_args: gcr_base_cflags,
)

frob_parser = executable('frob-parser',
  files('frob-parser.c'),
  dependencies: [ gcr_base_dep, gcr_base_deps ],
  c_args: gcr_base_cflags,
)
