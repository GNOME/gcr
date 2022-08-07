gcr_viewer_gtk4_sources = files(
  'gcr-certificate-widget.c',
  'gcr-section.c',
  'viewer.c',
)

gcr_viewer_gtk4_deps = [
  gck_dep,
  gcr_dep,
  p11kit_dep,
  gtk4_dep,
]

gcr_viewer_gtk4_cflags = [
  '-DGCK_API_SUBJECT_TO_CHANGE',
  '-DGCR_API_SUBJECT_TO_CHANGE',
  '-DP11_KIT_FUTURE_UNSTABLE_API',
]

gcr_viewer_gtk4 = executable('gcr-viewer-gtk4',
  gcr_viewer_gtk4_sources,
  dependencies: gcr_viewer_gtk4_deps,
  c_args: gcr_viewer_gtk4_cflags,
  include_directories: config_h_dir,
  install: true,
)
