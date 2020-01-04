subdir('icons')

gcr_ui_headers_install_dir = 'gcr-@0@'.format(gcr_major_version) / 'ui'

gcr_ui_public_sources = files(
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
)

gcr_ui_private_sources = files(
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
)

gcr_ui_headers = files(
  'gcr-ui.h',
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
)

# Generated sources
gcr_ui_enums_gen = gnome.mkenums_simple('gcr-enum-types',
  sources: gcr_ui_headers,
  install_header: true,
  install_dir: get_option('includedir') / gcr_ui_headers_install_dir,
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
  gcr_base_dep,
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

gcr_ui_symbolmap = meson.current_source_dir() / 'libgcr-ui.map'
gcr_ui_linkflags = cc.get_supported_link_arguments(
  '-Wl,--version-script,@0@'.format(gcr_ui_symbolmap),
)

# gcr-XX is just the same as gcr-ui-XX
gcr_ui_basename = 'gcr-ui-@0@'.format(gcr_major_version)
gcr_basename = 'gcr-@0@'.format(gcr_major_version)

gcr_ui_lib = shared_library(gcr_ui_basename,
  gcr_ui_sources,
  dependencies: gcr_ui_deps,
  c_args: gcr_ui_cflags,
  link_args: gcr_ui_linkflags,
  link_depends: gcr_ui_symbolmap,
  include_directories: config_h_dir,
  version: gcr_soversion,
  install: true,
)

gcr_ui_pkgconf_deps = [
  glib_dep,
  gio_dep,
  gobject_dep,
  'gck-@0@'.format(gck_major_version),
  'gcr-@0@'.format(gcr_major_version),
  gtk_dep,
]
pkgconfig.generate(gcr_ui_lib,
  subdirs: 'gcr-@0@'.format(gcr_major_version),
  requires: gcr_ui_pkgconf_deps,
  description: 'GObject and GUI library for high level crypto parsing and display',
)
pkgconfig.generate(gcr_ui_lib,
  filebase: gcr_basename,
  name: gcr_basename,
  subdirs: 'gcr-@0@'.format(gcr_major_version),
  requires: gcr_ui_pkgconf_deps,
  description: 'GObject and GUI library for high level crypto parsing and display',
)

install_headers(gcr_ui_headers,
  subdir: gcr_ui_headers_install_dir,
)

gcr_ui_dep = declare_dependency(
  link_with: gcr_ui_lib,
  sources: gcr_ui_enums_gen[1], # Make sure gcr-enum-types.h can be included
)

if get_option('introspection')
  gcr_ui_gir = gnome.generate_gir(gcr_ui_lib,
    sources: [ gcr_ui_headers, gcr_ui_public_sources ],
    namespace: 'GcrUi',
    nsversion: '@0@'.format(gcr_major_version),
    export_packages: 'gcr-ui-@0@'.format(gcr_major_version),
    identifier_prefix: 'Gcr',
    symbol_prefix: 'gcr',
    packages: gcr_ui_deps,
    includes: [
      'GObject-2.0',
      'Gio-2.0',
      'Gtk-3.0',
      gck_gir[0],
      gcr_gir[0],
    ],
    header: 'ui/gcr-ui.h',
    extra_args: [
      '-DGCR_COMPILATION',
      '-DGCR_API_SUBJECT_TO_CHANGE',
    ],
    install: true,
  )

  gcr_ui_vapi = gnome.generate_vapi('gcr-ui-@0@'.format(gcr_major_version),
    sources: gcr_ui_gir[0],
    packages: [
      'glib-2.0',
      'gio-2.0',
      'gck-@0@'.format(gck_major_version),
      'gcr-@0@'.format(gcr_major_version),
      'gtk+-3.0'
    ],
    metadata_dirs: meson.current_source_dir(),
    vapi_dirs: [
      build_root / 'gck',
      build_root / 'gcr',
    ],
    gir_dirs: [
      build_root / 'gck',
      build_root / 'gcr',
    ],
    install: true,
  )
endif

# gcr-viewer
gcr_viewer = executable('gcr-viewer',
  'gcr-viewer-tool.c',
  dependencies: [ gcr_ui_dep, gcr_ui_deps ],
  c_args: gcr_ui_cflags,
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
  dependencies: [ gcr_ui_dep, gcr_ui_deps ],
  c_args: gcr_ui_cflags,
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

# Register new mime types
install_data('gcr-crypto-types.xml',
  install_dir: get_option('datadir') / 'mime' / 'packages',
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

gcr_ui_test_cflags = [
  '-DSRCDIR="@0@"'.format(source_root),
]

foreach _test : gcr_ui_manual_tests
  test_bin = executable('test-'+_test,
    '@0@.c'.format(_test),
    dependencies: [ gcr_ui_deps, gcr_ui_dep ],
    c_args: [ gcr_ui_cflags, gcr_ui_test_cflags ],
    include_directories: config_h_dir,
  )
endforeach
