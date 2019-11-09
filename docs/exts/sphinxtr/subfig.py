"""
Adds subfigure functionality
"""

from docutils import nodes
import docutils.parsers.rst.directives as directives
from docutils.parsers.rst import Directive
from sphinx import addnodes

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
    figoutput = figoutput.replace('class="figure', 'style="width: %g%%" class="subfigure' % (float(node['width']) * 100))
    self.body = self.__body
    self.body.append(figoutput)

class subfigstart(nodes.General, nodes.Element):
    pass

def visit_subfigstart_tex(self, node):
    self.body.append('\n\\begin{figure}\n\\centering\n\\capstart\n')

def depart_subfigstart_tex(self, node):
    pass

def visit_subfigstart_html(self, node):
    atts = {'class': 'figure compound align-center'}
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
        width = self.options.get('width', None)
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

    app.connect('doctree-read', doctree_read)
