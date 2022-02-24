"""
Adds subfigure functionality
"""

import docutils
from docutils import nodes
import docutils.parsers.rst.directives as directives
from docutils.parsers.rst import Directive, Parser
from sphinx import addnodes
from docutils.parsers.rst.directives.images import Image
from docutils.statemachine import ViewList
from sphinx.util.nodes import nested_parse_with_titles

class subfig(nodes.General, nodes.Element):
    pass

def skip_visit(self, node):
    raise nodes.SkipNode

def visit_subfig_tex(self, node):
    self.__body = self.body
    self.body = []

def depart_subfig_tex(self, node):
    figoutput = ''.join(self.body)
    figoutput = figoutput.replace('[tbp]', '[t]{%s\\linewidth}' % node['width'])
    figoutput = figoutput.replace('figure', 'subfigure')
    self.body = self.__body
    self.body.append(figoutput)

def visit_subfig_html(self, node):
    self.__body = self.body
    self.body = []

def depart_subfig_html(self, node):
    figoutput = ''.join(self.body)
    figoutput = figoutput.replace('class="', 'class="align-center subsubfigure" style="width: %g%%"' % (float(node['width']) * 100))
    self.body = self.__body
    self.body.append(figoutput)

class subfigstart(nodes.General, nodes.Element):
    pass

def visit_subfigstart_tex(self, node):
    self.body.append('\n\\begin{figure}\n\\centering\n\\capstart\n')

def depart_subfigstart_tex(self, node):
    pass

def visit_subfigstart_html(self, node):
    atts = {'class': 'figure subfigure compound align-center'}
    self.body.append(self.starttag(node['subfigend'], 'div', **atts))

def depart_subfigstart_html(self, node):
    pass

class subfigend(nodes.General, nodes.Element):
    pass

def visit_subfigend_tex(self, node):
    pass

def depart_subfigend_tex(self, node):
    self.body.append('\n\n\\end{figure}\n\n')

def visit_subfigend_html(self, node):
    pass

def depart_subfigend_html(self, node):
    self.body.append('</div>')

class SubFigEndDirective(Directive):
    has_content = True
    optional_arguments = 3
    final_argument_whitespace = True

    option_spec = {'label': directives.uri,
                   'alt': directives.unchanged,
                   'width': directives.unchanged_required}

    def run(self):
        label = self.options.get('label', None)
        # width = self.options.get('width', 0.49)
        width = self.options.get('width', 0.95)
        alt = self.options.get('alt', None)

        node = subfigend('', ids=[label] if label is not None else [])

        if width is not None:
            node['width'] = width
        if alt is not None:
            node['alt'] = alt

        if self.content:
            anon = nodes.Element()
            self.state.nested_parse(self.content, self.content_offset, anon)
            first_node = anon[0]
            if isinstance(first_node, nodes.paragraph):
                caption = nodes.caption(first_node.rawsource, '',
                                        *first_node.children)
                node += caption

        if label is not None:
            targetnode = nodes.target('', '', ids=[label])
            node.append(targetnode)

        return [node]

class SubFigStartDirective(Directive):
    has_content = False
    optional_arguments = 0

    def run(self):
        node = subfigstart()
        return [node]

def doctree_read(app, doctree):
    secnums = app.builder.env.toc_secnumbers
    for node in doctree.traverse(subfigstart):
        parentloc = node.parent.children.index(node)

        subfigendloc = parentloc
        while subfigendloc < len(node.parent.children):
            n = node.parent.children[subfigendloc]
            if isinstance(n, subfigend):
                break
            subfigendloc += 1

        if subfigendloc == len(node.parent.children):
            return

        between_nodes = node.parent.children[parentloc:subfigendloc]
        subfigend_node = node.parent.children[subfigendloc]
        node['subfigend'] = subfigend_node
        for i, n in enumerate(between_nodes):
            if isinstance(n, nodes.figure):
                children = [n]
                prevnode = between_nodes[i-1]
                if isinstance(prevnode, nodes.target):
                    node.parent.children.remove(prevnode)
                    children.insert(0, prevnode)
                nodeloc = node.parent.children.index(n)
                node.parent.children[nodeloc] = subfig('', *children)
                node.parent.children[nodeloc]['width'] = subfigend_node['width']
                node.parent.children[nodeloc]['mainfigid'] = subfigend_node['ids'][0]


class Subfigure(Image):

    option_spec = Image.option_spec.copy()
    option_spec['caption'] = directives.unchanged
    option_spec['label'] = directives.uri

    has_content = True

    def run(self):
        self.options['width'] = '95%'
        label = self.options.get('label', None)

        (image_node,) = Image.run(self)
        if isinstance(image_node, nodes.system_message):
            return [image_node]

        figure_node = nodes.figure('', image_node, ids=[label] if label is not None else [])
        figure_node['align'] = 'center'

        if self.content:
            node = nodes.Element()          # anonymous container for parsing
            self.state.nested_parse(self.content, self.content_offset, node)
            first_node = node[0]
            if isinstance(first_node, nodes.paragraph):
                caption_node = nodes.caption(first_node.rawsource, '', *first_node.children)
                caption_node.source = first_node.source
                caption_node.line = first_node.line
                figure_node += caption_node
            elif not (isinstance(first_node, nodes.comment)
                      and len(first_node) == 0):
                error = self.state_machine.reporter.error(
                      'Subfigure caption must be a paragraph or empty comment.',
                      nodes.literal_block(self.block_text, self.block_text),
                      line=self.lineno)
                return [figure_node, error]
            if len(node) > 1:
                figure_node += nodes.legend('', *node[1:])
        else:
            rst = ViewList()
            rst.append(self.options['caption'], "", 0)

            parsed_node = nodes.section()
            parsed_node.document = self.state.document
            nested_parse_with_titles(self.state, rst, parsed_node)

            node = parsed_node[0]
            caption_node = nodes.caption(node.rawsource, '', *node.children)
            caption_node.source = node.source
            caption_node.line = node.line
            figure_node += caption_node

        return [figure_node]


def setup(app):
    app.add_node(subfigstart,
                 html=(visit_subfigstart_html, depart_subfigstart_html),
                 singlehtml=(visit_subfigstart_html, depart_subfigstart_html),
                 text=(skip_visit, None),
                 latex=(visit_subfigstart_tex, depart_subfigstart_tex))

    app.add_node(subfig,
                 html=(visit_subfig_html, depart_subfig_html),
                 singlehtml=(visit_subfig_html, depart_subfig_html),
                 text=(skip_visit, None),
                 latex=(visit_subfig_tex, depart_subfig_tex))

    app.add_node(subfigend,
                 html=(visit_subfigend_html, depart_subfigend_html),
                 singlehtml=(visit_subfigend_html, depart_subfigend_html),
                 text=(skip_visit, None),
                 latex=(visit_subfigend_tex, depart_subfigend_tex))

    app.add_directive('subfigstart', SubFigStartDirective)
    app.add_directive('subfigend', SubFigEndDirective)

    app.add_directive('subfigure', Subfigure)

    app.connect('doctree-read', doctree_read)
