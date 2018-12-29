gcr_public_sources = [
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
]

gcr_private_sources = [
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
]

gcr_headers = [
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
]

# Generated sources
gcr_version_conf = configuration_data()
gcr_version_conf.set('GCR_MAJOR', gcr_major_version)
gcr_version_conf.set('GCR_MINOR', gcr_minor_version)
gcr_version_conf.set('GCR_MICRO', gcr_micro_version)
gcr_version_h = configure_file(input: 'gcr-version.h.in',
  output: 'gcr-version.h',
  configuration: gcr_version_conf,
)

gcr_enums_gen = gnome.mkenums_simple('gcr-enum-types-base',
  sources: gcr_headers + [ gcr_version_h ],
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

gcr_sources = [
  gcr_private_sources,
  gcr_public_sources,
  gcr_enums_gen,
  gcr_marshal_gen,
  gcr_dbus_gen,
  gcr_oids,
]

gcr_deps = [
  glib_deps,
  p11kit_dep,
  libegg_dep,
  gck_dep,
]

gcr_cflags = [
 '-DG_LOG_DOMAIN="Gcr"',
 '-DGCR_COMPILATION',
 '-DGCR_API_SUBJECT_TO_CHANGE',
 '-DGCK_API_SUBJECT_TO_CHANGE',
 '-DP11_KIT_API_SUBJECT_TO_CHANGE',
]

gcr_lib = library('gcr-@0@'.format(gcr_major_version),
  gcr_sources,
  dependencies: gcr_deps,
  c_args: gcr_cflags,
  include_directories: config_h_dir,
  version: gcr_soversion,
)

gcr_dep = declare_dependency(
  link_with: gcr_lib,
  include_directories: include_directories('..'),
  sources: gcr_enums_gen[1], # Make sure gcr-enum-types-base.h can be included
)

if get_option('introspection')
  gcr_gir = gnome.generate_gir(gcr_lib,
    sources: gcr_public_sources,
    namespace: 'Gcr',
    nsversion: gcr_major_version,
    packages: gcr_deps,
    export_packages: 'gcr-@0@'.format(gcr_major_version),
    includes: [ 'GObject-2.0', 'Gio-2.0' ],
    extra_args: [
      '-DGCR_COMPILATION',
      '-DGCR_API_SUBJECT_TO_CHANGE',
      '--c-include=gcr/gcr.h'.format(meson.current_source_dir()),
    ],
    install: true,
  )

  gcr_vapi = gnome.generate_vapi('gcr-@0@'.format(gcr_major_version),
    sources: gcr_gir[0],
    packages: [ 'glib-2.0', 'gio-2.0' ],
    install: true,
  )
endif

# gcr-ssh-askpass binary
gcr_ssh_askpass = executable('gcr-ssh-askpass',
  'gcr-ssh-askpass.c',
  dependencies: [ glib_deps, libegg_dep ],
  c_args: [
    '-DGCR_SSH_ASKPASS_TOOL',
    '-DGCR_COMPILATION',
  ],
  include_directories: config_h_dir,
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
  '-D_GCR_TEST_SSH_ASKPASS_PATH="@0@"'.format(gcr_ssh_askpass.full_path()),
]

foreach _test : gcr_test_names
  test_bin = executable('test-'+_test,
    'test-@0@.c'.format(_test),
    dependencies: [ gcr_deps, gcr_dep ],
    link_with: [ gck_test_lib, egg_test_lib ],
    c_args: [ gcr_cflags, gcr_test_cflags ],
    include_directories: config_h_dir,
  )

  test(_test, test_bin,
    suite: 'gcr',
    depends: gcr_ssh_askpass,
  )
endforeach
