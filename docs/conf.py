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
copyright = '2020-2023, Ignis-Project'
author = 'Ömercan Yazici'


# -- General configuration ---------------------------------------------------

needs_sphinx = '2.4'

language = "en"

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
sys.path.append(os.path.abspath('exts'))
extensions = ['objectparameters', 'subfig', 'pythonfunc',
              'sphinx_design', 'sphinx_copybutton',
              'sphinx_last_updated_by_git',
              'nbsphinx']

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db',
                    '.DS_Store', 'README.md', '*/README.md']


# -- Options for HTML output -------------------------------------------------

html_theme = 'pydata_sphinx_theme'
html_logo = None
html_favicon = None
html_show_sourcelink = False
html_sourcelink_suffix = ""

html_theme_options = {
    "pygment_light_style": "manni",
    "pygment_dark_style": "material",
    "show_toc_level": 1,
    "show_nav_level": 2,
    "navbar_align": "content",
    "secondary_sidebar_items": ["page-toc"],
    "icon_links": [
        {
            "name": "GitHub",
            "url": "https://github.com/PearCoding/Ignis",
            "icon": "fa-brands fa-square-github",
            "type": "fontawesome",
        }
    ],
    "logo": {
        "text": "Ignis",
    }
}

html_static_path = ['_static']
html_css_files = [
    'css/custom.css',
    'css/lightbox.css'
]
html_js_files = [
    'js/lightbox.js'
]

html_sidebars = {
    '**': ["sidebar-nav-bs"]
}

source_suffix = ['.rst', '.md']

rst_prolog = r"""
.. role:: paramtype

.. role:: param_true

.. role:: param_false

.. role:: monosp

.. |texture| replace:: :paramtype:`texture`
.. |color| replace:: :paramtype:`color`
.. |number| replace:: :paramtype:`number`
.. |bool| replace:: :paramtype:`boolean`
.. |int| replace:: :paramtype:`integer`
.. |false| replace:: :param_false:`false`
.. |true| replace:: :param_true:`true`
.. |array| replace:: :paramtype:`array`
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
    # 'pointsize': '12pt',
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
