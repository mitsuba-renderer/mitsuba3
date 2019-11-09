"""
Fixes the table of contents in singlehtml mode so that section titles have the
correct section number in front.
"""

from docutils import nodes
from sphinx import addnodes

def stringize_secnum(secnum):
    return '.'.join(map(str, secnum))

def doctree_resolved(app, doctree, fromdocname):
    if app.builder.name == 'singlehtml':
        secnums = app.builder.env.toc_secnumbers
        for filenode in doctree.traverse(addnodes.start_of_file):
            docname = filenode['docname']
            if docname not in secnums:
                continue

            doc_secnums = secnums[docname]
            first_title_node = filenode.next_node(nodes.title)
            if first_title_node is not None and '' in doc_secnums:
                file_secnum = stringize_secnum(doc_secnums[''])
                title_text_node = first_title_node.children[0]
                newtext = file_secnum + ' ' + title_text_node.astext()
                first_title_node.replace(title_text_node, nodes.Text(newtext))

            for section_node in filenode.traverse(nodes.section):
                for id in section_node['ids']:
                    if '#' + id in doc_secnums:
                        subsection_num = stringize_secnum(doc_secnums['#' + id])
                        first_title_node = section_node.next_node(nodes.title)
                        if first_title_node is not None:
                            title_text_node = first_title_node.children[0]
                            newtext = subsection_num + ' ' + title_text_node.astext()
                            first_title_node.replace(title_text_node, nodes.Text(newtext))

def setup(app):
    app.connect('doctree-resolved', doctree_resolved)
