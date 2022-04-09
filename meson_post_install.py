#!/usr/bin/env python3

import os
import subprocess
import sys

# Env
install_prefix = os.environ['MESON_INSTALL_DESTDIR_PREFIX']

# Args
datadir = sys.argv[1]

icondir = os.path.join(install_prefix, datadir, 'icons', 'hicolor')
schemadir = os.path.join(install_prefix, datadir, 'glib-2.0', 'schemas')
mimedatabasedir = os.path.join(install_prefix, datadir, 'mime')

# We don't want to mess around when packaging environments
if os.environ.get('DESTDIR'):
  sys.exit(0)

print('Update icon cache...')
subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])

print('Compiling gsettings schemas...')
subprocess.call(['glib-compile-schemas', schemadir])

print('Updating MIME database...')
subprocess.call(['update-mime-database', mimedatabasedir])
