# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys

from sphinx.writers.html5 import HTML5Translator

# Work around an odd exception on readthedocs.org
vr = HTML5Translator.visit_reference
def replacement(self, node):
    if 'refuri' not in node and 'refid' not in node:
        print(node)
        return
    vr(self, node)
HTML5Translator.visit_reference = replacement

# -- Project information -----------------------------------------------------

project = 'Ignis'
copyright = '2020-2022, Ignis-Project'
author = 'Ömercan Yazici'


# -- General configuration ---------------------------------------------------

needs_sphinx = '2.4'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
sys.path.append(os.path.abspath('exts'))
extensions = ['objectparameters', 'subfig']

# Add any paths that contain templates here, relative to this directory.
templates_path = []

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'karma_sphinx_theme'
html_logo = None

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = []

html_theme_options = {}

html_sidebars = {
    '**': ['logo-text.html', 'globaltoc.html', 'searchbox.html']
}
html_show_sourcelink = False

source_suffix = '.rst'

rst_prolog = r"""
.. role:: paramtype

.. role:: monosp

.. |texture| replace:: :paramtype:`texture`
.. |color| replace:: :paramtype:`color`
.. |number| replace:: :paramtype:`number`
.. |bool| replace:: :paramtype:`boolean`
.. |int| replace:: :paramtype:`integer`
.. |false| replace:: :monosp:`false`
.. |true| replace:: :monosp:`true`
.. |string| replace:: :paramtype:`string`
.. |bsdf| replace:: :paramtype:`bsdf`
.. |vector| replace:: :paramtype:`vector`
.. |transform| replace:: :paramtype:`transform`
.. |entity| replace:: :paramtype:`entity`

.. |nbsp| unicode:: 0xA0
   :trim:
"""

latex_elements = {
#    'papersize': 'a4paper',
 #   'classoptions': ',english,twoside,singlespace',
    #'pointsize': '12pt',
    'preamble': r"""
\captionsetup{labelfont=bf}
\DeclareUnicodeCharacter{00A0}{}
""",

    'extrapackages': r"""
\usepackage{graphicx}
\usepackage[margin=8pt]{subcaption}
\usepackage{booktabs}
\usepackage{multirow}
\usepackage{longtable}
    """,
    
    # disable font inclusion
    'fontpkg': '',
    'fontenc': '',

    # disable fancychp
    'fncychap': '',

    # disable index printing
    'printindex': '',
}

latex_show_urls = 'footnote'
latex_use_parts = False
latex_domain_indices = False

# We use javascript instead of json as it is more powerful
primary_domain = None
highlight_language = 'javascript'