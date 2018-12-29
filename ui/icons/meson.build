large_icons = [
  'gcr-gnupg',
  'gcr-password',
  'gcr-smart-card',
]

small_icons = [
  'gcr-gnupg',
  'gcr-key',
  'gcr-key-pair',
  'gcr-password',
  'gcr-smart-card',
]

foreach size : [ 16, 22, 24, 32, 48 ]
  size_str = '@0@x@0@'.format(size)

  foreach icon : small_icons
    install_data(size_str / icon + '.png',
      install_dir: get_option('datadir') / 'icons' / 'hicolor' / size_str / 'apps',
    )
  endforeach
endforeach

foreach size : [ 256 ]
  size_str = '@0@x@0@'.format(size)

  foreach icon : large_icons
    install_data(size_str / icon + '.png',
      install_dir: get_option('datadir') / 'icons' / 'hicolor' / size_str / 'apps',
    )
  endforeach
endforeach
