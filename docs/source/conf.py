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

# Theme options for better readability
html_theme_options = {
    'navigation_depth': 4,
    'collapse_navigation': False,
    'sticky_navigation': True,
    'includehidden': True,
    'titles_only': False,
    'style_nav_header_background': '#2980B9',  # Blue header
}

# Custom CSS for better code block styling
html_css_files = [
    'custom.css',
]

# Logo and favicon (optional - add these files to _static/ if you have them)
# html_logo = '_static/logo.png'
# html_favicon = '_static/favicon.ico'

# Show "Edit on GitHub" link
html_context = {
    'display_github': True,
    'github_user': 'cmelnu',
    'github_repo': 'thunderos',
    'github_version': 'main',
    'conf_py_path': '/docs/source/',
}

# Syntax highlighting
pygments_style = 'monokai'  # Dark theme for code blocks

# -- Extension configuration -------------------------------------------------
todo_include_todos = True
