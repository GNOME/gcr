gcr_ui_public_sources = [
  'gcr-certificate-renderer.c',
  'gcr-certificate-widget.c',
  'gcr-collection-model.c',
  'gcr-combo-selector.c',
  'gcr-failure-renderer.c',
  'gcr-key-renderer.c',
  'gcr-key-widget.c',
  'gcr-import-button.c',
  'gcr-list-selector.c',
  'gcr-prompt-dialog.c',
  'gcr-renderer.c',
  'gcr-secure-entry-buffer.c',
  'gcr-tree-selector.c',
  'gcr-unlock-options-widget.c',
  'gcr-viewer.c',
  'gcr-viewer-widget.c',
]

gcr_ui_private_sources = [
  'eggimagemenuitem.c',
  'gcr-certificate-basics-widget.c',
  'gcr-certificate-details-widget.c',
  'gcr-certificate-exporter.c',
  'gcr-certificate-request-renderer.c',
  'gcr-dialog-util.c',
  'gcr-display-scrolled.c',
  'gcr-display-view.c',
  'gcr-gnupg-renderer.c',
  'gcr-live-search.c',
  'gcr-pkcs11-import-dialog.c',
  'gcr-pkcs11-import-interaction.c',
  'gcr-unlock-renderer.c',
  'gcr-viewer-window.c',
]

gcr_ui_headers = [
  'gcr-ui.h',
  'gcr-certificate-basics-widget.h',
  'gcr-certificate-details-widget.h',
  'gcr-certificate-renderer.h',
  'gcr-certificate-widget.h',
  'gcr-collection-model.h',
  'gcr-combo-selector.h',
  'gcr-deprecated.h',
  'gcr-failure-renderer.h',
  'gcr-key-renderer.h',
  'gcr-key-widget.h',
  'gcr-import-button.h',
  'gcr-list-selector.h',
  'gcr-prompt-dialog.h',
  'gcr-renderer.h',
  'gcr-secure-entry-buffer.h',
  'gcr-tree-selector.h',
  'gcr-unlock-options-widget.h',
  'gcr-viewer.h',
  'gcr-viewer-widget.h',
]

# Generated sources
gcr_ui_enums_gen = gnome.mkenums_simple('gcr-enum-types',
  sources: gcr_ui_headers,
)

gcr_ui_sources = [
  gcr_ui_private_sources,
  gcr_ui_public_sources,
  gcr_ui_enums_gen,
]

gcr_ui_deps = [
  glib_deps,
  p11kit_dep,
  libegg_dep,
  gck_dep,
  gcr_dep,
  gtk_dep,
  gtk_x11_dep,
]

gcr_ui_cflags = [
 '-DG_LOG_DOMAIN="Gcr"',
 '-DGCR_COMPILATION',
 '-DGCR_API_SUBJECT_TO_CHANGE',
 '-DGCK_API_SUBJECT_TO_CHANGE',
 '-DP11_KIT_API_SUBJECT_TO_CHANGE',
]

gcr_ui_lib = library('gcr-ui',
  gcr_ui_sources,
  dependencies: gcr_ui_deps,
  c_args: gcr_ui_cflags,
  include_directories: config_h_dir,
)

gcr_ui_dep = declare_dependency(
  link_with: gcr_ui_lib,
  include_directories: include_directories('..'),
)

# Manual UI testing programs
gcr_ui_manual_tests = [
  'frob-certificate',
  'frob-combo-selector',
  'frob-gnupg-selector',
  'frob-import-button',
  'frob-key',
  'frob-tree-selector',
  'frob-prompt',
  'frob-request',
  'frob-system-prompt',
  'frob-unlock',
  'frob-unlock-options',
]

foreach _test : gcr_ui_manual_tests
  test_bin = executable('test-'+_test,
    '@0@.c'.format(_test),
    dependencies: [ gcr_ui_deps, gcr_ui_dep ],
    c_args: gcr_cflags,
    include_directories: config_h_dir,
  )
endforeach
