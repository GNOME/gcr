gcr_ui_content_files = [
  'widget-gallery.md',
]

gcr_ui_toml = configure_file(
  input: 'gcr-ui.toml.in',
  output: '@BASENAME@',
  configuration: {
    'GCR_VERSION': gcr_api_version,
    'GCK_VERSION': gck_api_version,
  },
)

gcr_ui_docs = custom_target('gcr-ui-docs',
  input: gcr_ui_gir[0],
  output: gcr_ui_basename,
  command: [
    gi_docgen,
    'generate',
    '--quiet',
    '--fatal-warnings',
    '--add-include-path=@0@'.format(build_root / 'gck'),
    '--add-include-path=@0@'.format(build_root / 'gcr'),
    '--config', gcr_ui_toml,
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT@',
  ],
  depend_files: [ gcr_ui_toml, gcr_ui_content_files ],
  depends: [ gck_gir[0], gcr_gir[0] ],
  build_by_default: true,
  install: true,
  install_dir: get_option('datadir') / 'doc',
)
