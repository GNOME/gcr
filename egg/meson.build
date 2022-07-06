libegg_sources = [
  'egg-armor.c',
  'egg-asn1x.c',
  'egg-asn1-defs.c',
  'egg-buffer.c',
  'egg-dh.c',
  'egg-dn.c',
  'egg-decimal.c',
  'egg-file-tracker.c',
  'egg-hex.c',
  'egg-hkdf.c',
  'egg-libgcrypt.c',
  'egg-oid.c',
  'egg-padding.c',
  'egg-openssl.c',
  'egg-secure-memory.c',
  'egg-symkey.c',
  'egg-testing.c',
]

libegg_deps = [
  glib_deps,
  libgcrypt_dep,
]

libegg = static_library('egg',
  libegg_sources,
  dependencies: libegg_deps,
  include_directories: config_h_dir,
)

libegg_dep = declare_dependency(
  link_with: libegg,
  dependencies: libegg_deps,
)

# Tests
egg_test_lib = static_library('egg-test',
  sources: [ 'egg-testing.c', 'mock-interaction.c' ],
  dependencies: glib_deps,
  include_directories: config_h_dir,
)

egg_test_names = [
  'asn1',
  'asn1x',
  'dn',
  'decimal',
  'hex',
  'hkdf',
  'oid',
  'secmem',
  'padding',
  'symkey',
  'armor',
  'openssl',
  'dh',
]

egg_test_cflags = [
  '-DSRCDIR="@0@"'.format(source_root),
]

foreach _test : egg_test_names
  _test_name = 'egg-'+_test

  egg_test_bin = executable(_test_name,
    'test-@0@.c'.format(_test),
    link_with: egg_test_lib,
    dependencies: libegg_dep,
    c_args: egg_test_cflags,
    include_directories: config_h_dir,
  )

  test(_test, egg_test_bin,
    suite: 'egg',
  )
endforeach
