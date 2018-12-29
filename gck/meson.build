gck_sources_public = [
  'gck-attributes.c',
  'gck-dump.c',
  'gck-enumerator.c',
  'gck-misc.c',
  'gck-module.c',
  'gck-modules.c',
  'gck-object.c',
  'gck-object-cache.c',
  'gck-password.c',
  'gck-session.c',
  'gck-slot.c',
  'gck-uri.c',
]

gck_sources_private = [
  'gck-call.c',
  'gck-interaction.c',
]

gck_exported_headers = [
  'gck.h',
]

# Generated sources
gck_version_conf = configuration_data()
gck_version_conf.set_quoted('GCR_MAJOR', gck_major_version)
gck_version_conf.set_quoted('GCR_MINOR', gck_minor_version)
gck_version_conf.set_quoted('GCR_MICRO', gck_micro_version)
gck_version_h = configure_file(input: 'gck-version.h.in',
  output: 'gck-version.h',
  configuration: gck_version_conf,
)

gck_enums_gen = gnome.mkenums_simple('gck-enum-types',
  sources: gck_exported_headers,
)

gck_marshal_gen = gnome.genmarshal('gck-marshal',
  sources: 'gck-marshal.list',
  prefix: '_gck_marshal',
)

gck_sources = [
  gck_sources_public,
  gck_sources_private,
  gck_enums_gen,
  gck_marshal_gen,
]

gck_deps = [
  glib_deps,
  p11kit_dep,
  libegg_dep,
]

gck_cflags = [
  '-DG_LOG_DOMAIN="@0@"'.format('Gck'),
  '-DGCK_API_SUBJECT_TO_CHANGE',
  '-DP11_KIT_API_SUBJECT_TO_CHANGE',
  '-DPKCS11_REGISTRY_DIR="@0@"'.format(libdir / 'pkcs11'),
]

gck_lib = library('gck-@0@'.format(gck_major_version),
  gck_sources,
  dependencies: gck_deps,
  c_args: gck_cflags,
  include_directories: config_h_dir,
  version: gck_soversion,
  install: true,
)

gck_dep = declare_dependency(
  link_with: gck_lib,
  include_directories: include_directories('..'),
  sources: gck_enums_gen[1], # Make sure gck-enum-types.h can be included
)

# GObject intospection file
gck_gir_sources = [
  gck_sources_public,
  gck_exported_headers,
  gck_enums_gen[1],
]

if get_option('introspection')
  gck_gir = gnome.generate_gir(gck_lib,
    sources: gck_gir_sources,
    namespace: 'Gck',
    nsversion: gck_major_version,
    packages: gck_deps,
    export_packages: 'gck-@0@'.format(gck_major_version),
    includes: [ 'GObject-2.0', 'Gio-2.0' ],
    extra_args: [
      '-DGCK_COMPILATION',
      '-DGCK_API_SUBJECT_TO_CHANGE',
    '--c-include=gck/gck.h'.format(meson.current_source_dir()),
    ],
    install: true,
  )

  gck_vapi = gnome.generate_vapi('gck-@0@'.format(gck_major_version),
    sources: gck_gir[0],
    packages: [ 'glib-2.0', 'gio-2.0' ],
    install: true,
  )
endif

# tests
gck_test_lib = library('gck-testable',
  sources: [ 'gck-mock.c', 'gck-test.c' ],
  dependencies: [ gck_deps, gck_dep ],
  c_args: gck_cflags,
  include_directories: config_h_dir,
)

gck_mock_test_lib = shared_library('mock-test-module',
  sources: 'mock-test-module.c',
  link_with: gck_test_lib,
  dependencies: [ gck_deps, gck_dep ],
  c_args: gck_cflags,
  include_directories: config_h_dir,
)

gck_test_names = [
  'attributes',
  'module',
  'slot',
  'session',
  'object',
  'crypto',
  'uri',
  'enumerator',
  'modules',
]

gck_test_cflags = [
  '-D_GCK_TEST_MODULE_PATH="@0@"'.format(gck_mock_test_lib.full_path()),
]

foreach _test : gck_test_names
  _test_name = 'test-gck-'+_test

  gck_test_bin = executable(_test_name,
    '@0@.c'.format(_test_name),
    link_with: [ gck_test_lib, egg_test_lib ],
    dependencies: [ gck_deps, gck_dep ],
    c_args: gck_cflags + gck_test_cflags,
    include_directories: config_h_dir,
  )

  test(_test, gck_test_bin,
    suite: 'gck',
    depends: gck_mock_test_lib,
  )
endforeach

# pkg-config file
gck_pc_config = configuration_data()
gck_pc_config.set('prefix', gcr_prefix)
gck_pc_config.set('exec_prefix', '${prefix}')
gck_pc_config.set('libdir', join_paths('${exec_prefix}/lib'))
gck_pc_config.set('includedir', join_paths('${prefix}/include'))
gck_pc_config.set('datarootdir', join_paths('${prefix}/share'))
gck_pc_config.set('datadir', join_paths('${datarootdir}'))
gck_pc_config.set('sysconfdir', join_paths('${prefix}/etc'))
gck_pc_config.set('GCK_MAJOR', gck_major_version)
gck_pc_config.set('VERSION', gcr_version)
configure_file(input: 'gck.pc.in',
  output: 'gck.pc',
  configuration: gck_pc_config,
)
