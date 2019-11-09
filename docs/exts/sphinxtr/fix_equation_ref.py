"""
Fixes equation references from Sphinx math domain
from Equation (1) to Equation 1, which is what they
should be. Must be before sphinx.ext.math* in
extensions list.
"""

from docutils import nodes
import sphinx.ext.mathbase
from sphinx.ext.mathbase import displaymath, eqref

def number_equations(app, doctree, docname):
    num = 0
    numbers = {}
    for node in doctree.traverse(displaymath):
        if node['label'] is not None:
            num += 1
            node['number'] = num
            numbers[node['label']] = num
        else:
            node['number'] = None
    for node in doctree.traverse(eqref):
        if node['target'] not in numbers:
            continue
        num = '%d' % numbers[node['target']]
        node[0] = nodes.Text(num, num)

sphinx.ext.mathbase.number_equations = number_equations

def setup(app):
    pass
