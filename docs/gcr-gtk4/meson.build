gcr_gtk4_toml = configure_file(
  input: 'gcr-gtk4.toml.in',
  output: '@BASENAME@',
  configuration: {
    'GCR_VERSION': gcr_api_version,
    'GCK_VERSION': gck_api_version,
  },
)

gcr_gtk4_docs = custom_target('gcr-gtk4-docs',
  input: gcr_gtk4_gir[0],
  output: gcr_gtk4_basename,
  command: [
    gi_docgen,
    'generate',
    '--quiet',
    '--fatal-warnings',
    '--add-include-path=@0@'.format(build_root / 'gck'),
    '--add-include-path=@0@'.format(build_root / 'gcr'),
    '--config', gcr_gtk4_toml,
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT@',
  ],
  depend_files: [ gcr_gtk4_toml ],
  depends: [ gck_gir[0], gcr_gir[0] ],
  build_by_default: true,
  install: true,
  install_dir: get_option('datadir') / 'doc',
)
