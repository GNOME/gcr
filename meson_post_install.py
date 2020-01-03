#!/usr/bin/env python3

import os
import subprocess
import sys

# Env
install_prefix = os.environ['MESON_INSTALL_DESTDIR_PREFIX']

# Args
datadir = sys.argv[1]
libdir = sys.argv[2]
gcr_major_version = sys.argv[3]
gcr_soversion = sys.argv[4]

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

# FIXME: after a major version bump, just drop this
print('Creating symlink for libgcr-{}.so'.format(gcr_major_version))

def _get_path_for_lib(basename):
    return os.path.join(install_prefix, libdir, basename)

libgcr_ui_basename = 'libgcr-ui-{}.so'.format(gcr_major_version)
libgcr_basename = 'libgcr-{}.so'.format(gcr_major_version)

subprocess.call(['ln', '-f', '-s', libgcr_ui_basename, _get_path_for_lib(libgcr_basename)])

for v in gcr_soversion.split('.'):
    libgcr_ui_basename += '.{}'.format(v)
    libgcr_basename += '.{}'.format(v)

    if not os.path.exists(_get_path_for_lib(libgcr_ui_basename)):
        continue

    subprocess.call(['ln', '-f', '-s', libgcr_ui_basename, _get_path_for_lib(libgcr_basename)])
