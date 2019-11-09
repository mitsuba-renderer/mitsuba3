import re
from docutils import nodes
import sphinx.writers.html

BaseTranslator = sphinx.writers.html.HTMLTranslator

class CustomHTMLTranslator(BaseTranslator):
    def visit_tabular_col_spec(self, node):
        self.table_spec = re.split(r'[\s\|]+', node['spec'])
        raise nodes.SkipNode

    def bulk_text_processor(self, text):
        if '~' in text:
            text = text.replace('~', '&nbsp;')
        return text

    def visit_entry(self, node):
        atts = {'class': []}
        if isinstance(node.parent.parent, nodes.thead):
            atts['class'].append('head')
        if node.parent.parent.parent.stubs[node.parent.column]:
            # "stubs" list is an attribute of the tgroup element
            atts['class'].append('stub')
        if atts['class']:
            tagname = 'th'
            atts['class'] = ' '.join(atts['class'])
        else:
            tagname = 'td'
            del atts['class']
        table_spec = getattr(self, 'table_spec', None)
        if (tagname == 'td' or tagname == 'th') and table_spec is not None:
            if len(table_spec) > node.parent.column:
                colspec = table_spec[node.parent.column]

                horiz_align = ''
                vert_align = ''

                if 'raggedright' in colspec or colspec == 'l':
                    horiz_align = ' align-left'
                elif 'raggedleft' in colspec or colspec == 'r':
                    horiz_align = ' align-right'
                elif 'center' in colspec or colspec == 'c':
                    horiz_align = ' align-center'

                if 'p{' in colspec:
                    vert_align = ' align-top'
                elif 'm{' in colspec:
                    vert_align = ' align-middle'
                elif 'b{' in colspec:
                    vert_align = ' align-bottom'

                align_type = {'l': 'left',
                              'r': 'right',
                              'c': 'center'}
                atts['class'] = (atts.get('class', '') + horiz_align + vert_align)
        node.parent.column += 1
        if 'morerows' in node:
            atts['rowspan'] = node['morerows'] + 1
        if 'morecols' in node:
            atts['colspan'] = node['morecols'] + 1
            node.parent.column += node['morecols']
        self.body.append(self.starttag(node, tagname, '', **atts))
        self.context.append('</%s>\n' % tagname.lower())
        if len(node) == 0:              # empty cell
            self.body.append('&nbsp;')
        self.set_first_last(node)

sphinx.writers.html.SmartyPantsHTMLTranslator = CustomHTMLTranslator
