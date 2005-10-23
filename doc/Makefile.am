man_MANS = bbe.1
info_TEXINFOS = bbe.texi
CLEANFILES = bbe.html

docdir = $(datadir)/doc
doc_DATA = bbe.html
AM_MAKEINFOHTMLFLAGS=--no-split

bbe.html: bbe.texi bbe.info

html: bbe.html

.PHONY: html

EXTRA_DIST = $(man_MANS) $(info_TEXINFOS) $(doc_DATA)
