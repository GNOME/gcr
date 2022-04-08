gck_content_files = [
  'pkcs11-links.md',
]

gck_toml = configure_file(
  input: 'gck.toml.in',
  output: 'gck.toml',
  configuration: {
    'GCK_VERSION': gck_api_version,
  },
)

gck_docs = custom_target('gck-docs',
  input: gck_gir[0],
  output: gck_basename,
  command: [
    gi_docgen,
    'generate',
    '--quiet',
    '--fatal-warnings',
    '--config', gck_toml,
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT@',
  ],
  depend_files: [
    gck_toml,
    gck_content_files,
  ],
  build_by_default: true,
  install: true,
  install_dir: get_option('datadir') / 'doc',
)
