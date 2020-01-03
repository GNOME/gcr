version_xml = configure_file(
  input: 'version.xml.in',
  output: '@BASENAME@',
  configuration: { 'VERSION': gck_version },
)

gck_docs_ignore_headers = [
  'gck-private.h',
  'test-suite.h',
  'gck-mock.h',
  'gck-test.h',
  'pkcs11-trust-assertions.h',
  'pkcs11.h',
  'pkcs11n.h',
  'pkcs11x.h',
  'pkcs11i.h',
]

gnome.gtkdoc('gck',
  main_sgml: 'gck-docs.sgml',
  content_files: files(
    'gck-pkcs11-links.xml',
  ),
  src_dir: [
    include_directories('../../../gck'),
  ],
  ignore_headers: gck_docs_ignore_headers,
  dependencies: gck_dep,
  gobject_typesfile: 'gck.types',
  scan_args: [
    '--rebuild-types',
    '--deprecated-guards=GCK_DISABLE_DEPRECATED',
  ],
  install: true,
)

