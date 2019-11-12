"""
Adds a new directive called 'figtable' that creates a figure
around a table.
"""

import docutils
from docutils import nodes
import docutils.parsers.rst.directives as directives
from docutils.parsers.rst import Directive
from sphinx import addnodes

class figtable(nodes.General, nodes.Element):
    pass

def visit_figtable_node(self, node):
    pass

def depart_figtable_node(self, node):
    pass

def visit_figtable_tex(self, node):
    if node['nofig']:
        self.body.append('\n\n\\begin{table}\n\\capstart\n\\begin{center}\n')
    else:
        self.body.append('\n\n\\begin{figure}[tbp]\n\\capstart\n\\begin{center}\n')

def depart_figtable_tex(self, node):
    if node['nofig']:
        self.body.append('\n\\end{center}\n\\end{table}\n')
    else:
        self.body.append('\n\\end{center}\n\\end{figure}\n')

def visit_figtable_html(self, node):
    atts = {'class': 'figure align-center'}
    self.body.append(self.starttag(node, 'div', **atts) + '<center>')

def depart_figtable_html(self, node):
    self.body.append('</center></div>')

class FigTableDirective(Directive):

    has_content = True
    optional_arguments = 5
    final_argument_whitespace = True

    option_spec = {'label': directives.uri,
                   'spec': directives.unchanged,
                   'caption': directives.unchanged,
                   'alt': directives.unchanged,
                   'nofig': directives.flag}

    def run(self):
        label = self.options.get('label', None)
        spec = self.options.get('spec', None)
        caption = self.options.get('caption', None)
        alt = self.options.get('alt', None)
        nofig = 'nofig' in self.options

        figtable_node = figtable('', ids=[label] if label is not None else [])
        figtable_node['nofig'] = nofig

        if spec is not None:
            table_spec_node = addnodes.tabular_col_spec()
            table_spec_node['spec'] = spec
            figtable_node.append(table_spec_node)

        node = nodes.Element()
        self.state.nested_parse(self.content, self.content_offset, node)
        tablenode = node[0]
        if alt is not None:
            tablenode['alt'] = alt
        figtable_node.append(tablenode)

        if caption is not None:
            # Original SphinxTr source:
            # caption_node = nodes.caption('', '', nodes.Text(caption))
            # figtable_node.append(caption_node)

            # Modified: Support for parsing content of captions
            caption_node = nodes.Element()
            self.state.nested_parse(docutils.statemachine.StringList([caption]), self.content_offset, caption_node)
            if isinstance(caption_node[0], nodes.paragraph):
                caption = nodes.caption(caption_node[0].rawsource, '', *caption_node[0].children)
                figtable_node += caption

        if label is not None:
            targetnode = nodes.target('', '', ids=[label])
            figtable_node.append(targetnode)

        return [figtable_node]

def setup(app):
    app.add_node(figtable,
                 html=(visit_figtable_html, depart_figtable_html),
                 singlehtml=(visit_figtable_html, depart_figtable_html),
                 latex=(visit_figtable_tex, depart_figtable_tex),
                 text=(visit_figtable_node, depart_figtable_node))

    app.add_directive('figtable', FigTableDirective)
