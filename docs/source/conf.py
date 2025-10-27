# Configuration file for the Sphinx documentation builder.

# -- Project information -----------------------------------------------------
project = 'ThunderOS'
copyright = '2025, ThunderOS Team'
author = 'ThunderOS Team'
release = '0.1.0'

# License information
project_license = 'GPL v3'

# -- General configuration ---------------------------------------------------
extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.viewcode',
    'sphinx.ext.todo',
]

templates_path = ['_templates']
exclude_patterns = []

# -- Options for HTML output -------------------------------------------------
html_theme = 'sphinx_rtd_theme'
html_static_path = ['_static']

# -- Extension configuration -------------------------------------------------
todo_include_todos = True
