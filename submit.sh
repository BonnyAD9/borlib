#!/usr/bin/sh

set -e

rm xstigl00.zip || true

typst c doc/document/main.typ doc/document/xstigl00-borlib-documentation.pdf
typst c doc/slides/main.typ doc/slides/xstigl00-borlib-presentation.pdf

zip xstigl00.zip -r src Makefile CMakeLists.txt \
    doc/document/documentation.pdf doc/slides/presentation.pdf \
    doc/slides/presentation.mp4 README.md