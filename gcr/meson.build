gcr_headers_subdir = 'gcr-@0@'.format(gcr_api_version)
gcr_headers_install_dir = gcr_headers_subdir / 'gcr'

gcr_public_sources = files(
  'gcr-certificate.c',
  'gcr-certificate-chain.c',
  'gcr-certificate-field.c',
  'gcr-certificate-request.c',
  'gcr-certificate-section.c',
  'gcr-fingerprint.c',
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
  'gcr-ssh-askpass.c',
  'gcr-system-prompt.c',
  'gcr-system-prompter.c',
  'gcr-trust.c',
)

gcr_private_sources = files(
  'gcr-callback-output-stream.c',
  'gcr-certificate-extensions.c',
  'gcr-gnupg-importer.c',
  'gcr-gnupg-process.c',
  'gcr-gnupg-records.c',
  'gcr-key-mechanisms.c',
  'gcr-openpgp.c',
  'gcr-openssh.c',
  'gcr-pkcs11-importer.c',
  'gcr-record.c',
  'gcr-subject-public-key.c',
  'gcr-util.c',
)

if with_gcrypt
  gcr_private_sources += files(
    'gcr-parser-libgcrypt.c',
  )
elif with_gnutls
  gcr_private_sources += files(
    'gcr-parser-gnutls.c',
  )
endif

gcr_headers = files(
  'gcr.h',
  'gcr-certificate.h',
  'gcr-certificate-chain.h',
  'gcr-certificate-field.h',
  'gcr-certificate-request.h',
  'gcr-certificate-section.h',
  'gcr-fingerprint.h',
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
  'gcr-ssh-askpass.h',
  'gcr-system-prompt.h',
  'gcr-system-prompter.h',
  'gcr-trust.h',
  'gcr-types.h',
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

gcr_headers += gcr_version_h

gcr_enums_gen = gnome.mkenums_simple('gcr-enum-types',
  sources: gcr_headers,
  install_header: true,
  install_dir: get_option('includedir') / gcr_headers_install_dir,
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

gcr_symbolmap = meson.current_source_dir() / 'libgcr.map'
gcr_linkflags = cc.get_supported_link_arguments(
  '-Wl,--version-script,@0@'.format(gcr_symbolmap),
)

gcr_lib = shared_library(gcr_basename,
  gcr_sources,
  dependencies: gcr_deps,
  c_args: gcr_cflags,
  link_args: gcr_linkflags,
  link_depends: gcr_symbolmap,
  include_directories: config_h_dir,
  version: gcr_version,
  soversion: gcr_soversion,
  install: true,
)

install_headers(gcr_headers,
  subdir: gcr_headers_install_dir,
)

gcr_dep = declare_dependency(
  link_with: gcr_lib,
  include_directories: include_directories('..'),
  sources: [
    gcr_enums_gen[1],
    gcr_oids[1],
    gcr_marshal_gen[1],
  ],
)

if get_option('introspection')
  gcr_gir = gnome.generate_gir(gcr_lib,
    sources: [ gcr_public_sources, gcr_headers ],
    namespace: 'Gcr',
    nsversion: gcr_api_version,
    export_packages: gcr_basename,
    includes: [
      'GObject-2.0',
      'Gio-2.0',
      gck_gir[0],
    ],
    header: 'gcr/gcr.h',
    extra_args: [
      '-DGCR_COMPILATION',
      '-DGCR_API_SUBJECT_TO_CHANGE',
    ],
    install: true,
  )

  if enable_vapi
    gcr_vapi = gnome.generate_vapi(gcr_basename,
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
endif

# pkg-config file
pkgconfig.generate(gcr_lib,
  subdirs: gcr_headers_subdir,
  requires: [ glib_dep, gio_dep, gobject_dep, p11kit_dep, gck_lib ],
  description: 'GObject and GUI library for high level crypto parsing and display',
)

# gcr4-ssh-askpass binary
gcr_ssh_askpass = executable('gcr4-ssh-askpass',
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

if get_option('ssh_agent')
  # gcr-ssh-agent binary
  gcr_ssh_agent_lib_sources = [
    'gcr-ssh-agent-interaction.c',
    'gcr-ssh-agent-preload.c',
    'gcr-ssh-agent-process.c',
    'gcr-ssh-agent-service.c',
    'gcr-ssh-agent-util.c',
  ]

  gcr_ssh_agent = executable('gcr-ssh-agent',
    gcr_ssh_agent_lib_sources + [ 'gcr-ssh-agent.c' ],
    dependencies: [ gcr_deps, gcr_dep, libsecret_dep, libsystemd_deps ],
    c_args: [
      '-DGCR_COMPILATION',
      '-DGCR_API_SUBJECT_TO_CHANGE',
    ],
    include_directories: config_h_dir,
    install: true,
    install_dir: get_option('libexecdir'),
  )

  if with_systemd
    configure_file(
      input: 'gcr-ssh-agent.service.in',
      output: '@BASENAME@',
      configuration: {
        'libexecdir': gcr_prefix / get_option('libexecdir'),
      },
      install_dir: systemduserunitdir,
    )
    systemctl = find_program('systemctl', required: false)
    if systemctl.found()
        systemctl = systemctl.full_path()
    else
        systemctl = '/usr/bin/systemctl'
    endif
    configure_file(
      input: 'gcr-ssh-agent.socket.in',
      output: '@BASENAME@',
      configuration: {
        'systemctl': systemctl,
      },
      install_dir: systemduserunitdir,
    )
  endif

  gcr_ssh_agent_test_cflags = [
    '-DSRCDIR="@0@"'.format(source_root),
    '-D_GCR_TEST_SSH_ASKPASS_PATH="@0@"'.format(gcr_ssh_askpass.full_path()),
  ]

  gcr_ssh_agent_test_lib = static_library('gcr-ssh-agent-test',
    sources: gcr_ssh_agent_lib_sources + [ 'gcr-ssh-agent-test.c' ],
    dependencies: [ gcr_deps, gcr_dep, libsecret_dep, libsystemd_deps ],
    c_args: [
      '-DGCR_COMPILATION',
      '-DGCR_API_SUBJECT_TO_CHANGE',
    ] + gcr_ssh_agent_test_cflags,
    include_directories: config_h_dir,
  )

  gcr_ssh_agent_test_names = [
    'preload',
    'process',
    'service',
    'util',
  ]

  foreach _test : gcr_ssh_agent_test_names
    test_bin = executable('test-ssh-agent-'+_test,
      'test-ssh-agent-@0@.c'.format(_test),
      dependencies: [ gcr_deps, gcr_dep ],
      link_with: [ gcr_ssh_agent_test_lib, egg_test_lib ],
      c_args: [ gcr_cflags, gcr_ssh_agent_test_cflags ],
      include_directories: config_h_dir,
    )

    test(_test, test_bin,
      suite: 'gcr-ssh-agent',
      depends: gcr_ssh_askpass,
    )
  endforeach
endif

# Tests
gcr_test_names = [
  'util',
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

# Example frob programs
frob_certificate_request = executable('frob-certificate-request',
  files('frob-certificate-request.c', 'console-interaction.c'),
  dependencies: [ gcr_dep, gcr_deps ],
  c_args: gcr_cflags,
)

frob_openpgp = executable('frob-openpgp',
  files('frob-openpgp.c'),
  dependencies: [ gcr_dep, gcr_deps ],
  c_args: gcr_cflags,
)

frob_parser = executable('frob-parser',
  files('frob-parser.c'),
  dependencies: [ gcr_dep, gcr_deps ],
  c_args: gcr_cflags,
)
