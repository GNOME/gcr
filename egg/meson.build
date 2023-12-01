libegg_sources = [
  'egg-armor.c',
  'egg-asn1x.c',
  'egg-asn1-defs.c',
  'egg-buffer.c',
  'egg-decimal.c',
  'egg-dh.c',
  'egg-dn.c',
  'egg-file-tracker.c',
  'egg-hex.c',
  'egg-oid.c',
  'egg-padding.c',
  'egg-secure-memory.c',
  'egg-testing.c',
]

libegg_deps = [
  glib_deps,
  crypto_deps,
]

if with_gcrypt
  libegg_sources += [
    'egg-crypto-libgcrypt.c',
    'egg-dh-libgcrypt.c',
    'egg-hkdf-libgcrypt.c',
    'egg-libgcrypt.c',
    'egg-openssl.c',
    'egg-symkey.c',
  ]
elif with_gnutls
  libegg_sources += [
    'egg-crypto-gnutls.c',
    'egg-dh-gnutls.c',
    'egg-hkdf-gnutls.c',
  ]
endif

libegg = static_library('egg',
  libegg_sources,
  dependencies: libegg_deps,
  include_directories: config_h_dir,
)

libegg_dep = declare_dependency(
  link_with: libegg,
  dependencies: libegg_deps + crypto_deps,
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
  'armor',
  'dh',
]

egg_libgcrypt_test_names = [
  'symkey',
  'openssl',
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

if with_gcrypt
  foreach _test : egg_libgcrypt_test_names
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
endif
