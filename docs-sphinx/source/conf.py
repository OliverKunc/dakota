# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys
sys.path.append(os.path.abspath("./_pygments"))

from lexer.dakota import DakotaLexer

# -- Project information -----------------------------------------------------

project = 'dakota'
copyright = '2022, Sandia National Laboratories'
author = 'Sandia National Laboratories'

# The full version, including alpha/beta/rc tags
release = '6.16.0'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = ['myst_parser', 'sphinxcontrib.bibtex']

# Dakota input file syntax highlighting
highlight_language = "dakota"
pygments_style = 'style.dakota.DakotaStyle'

source_suffix = {
    '.rst': 'restructuredtext',
    '.md': 'markdown'
}

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []

# -- Options for BibTeX citation input ---------------------------------------
bibtex_bibfiles = ['../../docs/Dakota.bib']

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme'

html_theme_options = {
    'navigation_depth': -1,
}

html_css_files = [
    'css/dakota_theme.css',
    'css/sandiaheaderlite.css'
]

html_logo = 'img/dakota_Arrow_Name_Tag_horiz_transparent.png'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']
