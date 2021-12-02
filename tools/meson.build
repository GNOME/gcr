# gcr-viewer
gcr_viewer = executable('gcr-viewer',
  'gcr-viewer-tool.c',
  'gcr-viewer-window.c',
  dependencies: [ gcr_gtk3_dep, gcr_gtk3_deps ],
  c_args: gcr_gtk3_cflags,
  include_directories: config_h_dir,
  install: true,
)

i18n.merge_file(
  input: configure_file(
    input: 'gcr-viewer.desktop.in.in',
    output: '@BASENAME@',
    configuration: { 'bindir': gcr_prefix / get_option('bindir') },
  ),
  output: '@BASENAME@',
  type: 'desktop',
  po_dir: podir,
  install: true,
  install_dir: get_option('datadir') / 'applications',
)

# gcr-prompter
gcr_prompter = executable('gcr-prompter',
  'gcr-prompter-tool.c',
  dependencies: [ gcr_gtk3_dep, gcr_gtk3_deps ],
  c_args: gcr_gtk3_cflags,
  include_directories: config_h_dir,
  install: true,
  install_dir: get_option('libexecdir'),
)

i18n.merge_file(
  input: configure_file(
    input: 'gcr-prompter.desktop.in.in',
    output: '@BASENAME@',
    configuration: { 'libexecdir': gcr_prefix / get_option('libexecdir') },
  ),
  output: '@BASENAME@',
  type: 'desktop',
  po_dir: podir,
  install: true,
  install_dir: get_option('datadir') / 'applications',
)


# Manual UI testing programs
gcr_tools_manual_tests = [
  'frob-unlock',
]

gcr_tools_test_cflags = [
  '-DSRCDIR="@0@"'.format(source_root),
]

foreach _test : gcr_tools_manual_tests
  test_bin = executable('test-'+_test,
    '@0@.c'.format(_test),
    'gcr-viewer-window.c',
    dependencies: [ gcr_gtk3_deps, gcr_gtk3_dep ],
    c_args: [ gcr_gtk3_cflags, gcr_tools_test_cflags ],
    include_directories: config_h_dir,
  )
endforeach
