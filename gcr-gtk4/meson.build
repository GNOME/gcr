gcr_gtk4_headers_install_dir = gcr_headers_subdir / 'gcr-gtk4'

gcr_gtk4_public_sources = files(
  'gcr-certificate-widget.c',
)

gcr_gtk4_private_sources = files(
  'gcr-section.c',
)

gcr_gtk4_headers = files(
  'gcr-certificate-widget.h',
)


gcr_gtk4_enums_gen = gnome.mkenums_simple('gcr-gtk4-enum-types',
  sources: gcr_gtk4_headers,
  install_header: true,
  install_dir: get_option('includedir') / gcr_gtk4_headers_install_dir,
)

gcr_gtk4_sources = [
  gcr_gtk4_private_sources,
  gcr_gtk4_public_sources,
  gcr_gtk4_enums_gen,
]

gcr_gtk4_deps = [
  glib_deps,
  p11kit_dep,
  libegg_dep,
  gck_dep,
  gcr_dep,
  gtk4_dep,
]

gcr_gtk4_cflags = [
 '-DG_LOG_DOMAIN="GcrGtk4"',
 '-DGCR_COMPILATION',
 '-DGCR_API_SUBJECT_TO_CHANGE',
 '-DGCK_API_SUBJECT_TO_CHANGE',
 '-DP11_KIT_API_SUBJECT_TO_CHANGE',
]

gcr_gtk4_symbolmap = meson.current_source_dir() / 'libgcr-gtk4.map'
gcr_gtk4_linkflags = cc.get_supported_link_arguments(
  '-Wl,--version-script,@0@'.format(gcr_gtk4_symbolmap),
)

gcr_gtk4_lib = shared_library(gcr_gtk4_basename,
  gcr_gtk4_sources,
  dependencies: gcr_gtk4_deps,
  c_args: gcr_gtk4_cflags,
  link_args: gcr_gtk4_linkflags,
  link_depends: gcr_gtk4_symbolmap,
  include_directories: config_h_dir,
  version: gcr_soversion,
  install: true,
)

gcr_gtk4_pkgconf_deps = [
  glib_dep,
  gio_dep,
  gobject_dep,
  gck_lib,
  gcr_lib,
  gtk4_dep,
]

pkgconfig.generate(gcr_gtk4_lib,
  subdirs: gcr_headers_subdir,
  requires: gcr_gtk4_pkgconf_deps,
  description: 'Library providing Gtk4 widgets for high level crypto parsing and display',
)

install_headers([gcr_gtk4_headers ],
  subdir: gcr_gtk4_headers_install_dir,
)

gcr_gtk4_dep = declare_dependency(
  link_with: gcr_gtk4_lib,
  sources: gcr_gtk4_enums_gen[1], # Make sure gcr-enum-types.h can be included
)

if get_option('introspection')
  gcr_gtk4_gir = gnome.generate_gir(gcr_gtk4_lib,
    sources: [ gcr_gtk4_headers, gcr_gtk4_public_sources ],
    namespace: 'GcrGtk4',
    nsversion: gcr_api_version,
    export_packages: gcr_gtk4_basename,
    identifier_prefix: 'Gcr',
    symbol_prefix: 'gcr',
    includes: [
      'GObject-2.0',
      'Gio-2.0',
      'Gtk-4.0',
      gck_gir[0],
      gcr_gir[0],
    ],
    header: 'gcr-gtk4/gcr-gtk4.h',
    extra_args: [
      '-DGCR_COMPILATION',
      '-DGCR_API_SUBJECT_TO_CHANGE',
    ],
    install: true,
  )

  gcr_gtk4_vapi = gnome.generate_vapi(gcr_gtk4_basename,
    sources: gcr_gtk4_gir[0],
    packages: [
      'glib-2.0',
      'gio-2.0',
      gck_vapi,
      gcr_vapi,
      'gtk4'
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
