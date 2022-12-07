GCR
===
GCR is a library for displaying certificates and crypto UI, accessing
key stores. It also provides the viewer for crypto files on the GNOME
desktop.

GCK is a library for accessing PKCS#11 modules like smart cards, in a
(G)object oriented way.

Building
--------

You can build GCR using [Meson] with the following build commands (replace
`$BUILDDIR` with your chosed build directory).

```
$ meson $BUILDDIR
$ meson compile -C $BUILDDIR
$ meson install -C $BUILDDIR
```

Contributing
------------
The code and issue tracker of GCR can be found at the GNOME GitLab instance at
https://gitlab.gnome.org/GNOME/gcr.

If you would like to get involved with GNOME projects, please also visit our
[Newcomers page] on the Wiki.

Documentation
-------------
The documentation for GCR and GCK is built using [gi-docgen].

You can find the nightly documentation at:

* Gck: https://gnome.pages.gitlab.gnome.org/gcr/gck-2/
* Gcr: https://gnome.pages.gitlab.gnome.org/gcr/gcr-4/

You can find the older GCR documentation at:

* Gck: https://gnome.pages.gitlab.gnome.org/gcr/gck-1/
* Gcr: https://gnome.pages.gitlab.gnome.org/gcr/gcr-3/
* GcrUI: https://gnome.pages.gitlab.gnome.org/gcr/gcr-ui-3/

Debug tracing
-------------
The Gcr and Gck libraries contain statements which help debug flow
and logic. In many cases these help you track down problems.

Use the environment variable `G_MESSAGES_DEBUG='all'` or
`G_MESSAGES_DEBUG='xxx'` to display either all messages or a specific categories
of debug messages. You can separate categories in this list with spaces, commas
or semicolons. Gcr library uses category 'Gcr', while Gck library uses category
'Gck'.

```
# Example to display all debug messages:
$ G_MESSAGES_DEBUG=all gcr-viewer /path/to/certificate.crt

# Example to display debug messages for a specific category:
$ G_MESSAGES_DEBUG="Gcr" gcr-viewer /path/to/certificate.crt
```

For the Gck debug messages simply replace 'Gcr' with 'Gck' in the above
examples.

More information
----------------
To discuss issues with developers and other users, you can post to the
[GNOME Discourse instance](https://discourse.gnome.org).



[gi-docgen]: https://gnome.pages.gitlab.gnome.org/gi-docgen/
[Meson]: https://mesonbuild.com
[Newcomers page]: https://wiki.gnome.org/TranslationProject/JoiningTranslation
