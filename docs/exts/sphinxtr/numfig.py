from docutils import nodes
from sphinx.roles import XRefRole
import figtable
import subfig
from custombackports import OrderedDict, OrderedSet

# Element classes

class page_ref(nodes.reference):
    pass

class num_ref(nodes.reference):
    pass


# Visit/depart functions

def skip_page_ref(self, node):
    raise nodes.SkipNode

def latex_visit_page_ref(self, node):
    self.body.append("\\pageref{%s:%s}" % (node['refdoc'], node['reftarget']))
    raise nodes.SkipNode

def latex_visit_num_ref(self, node):
    fields = node['reftarget'].split('#')

    if len(fields) > 1:
        label, target = fields
    else:
        label = None
        target = fields[0]

    if target not in self.builder.env.docnames_by_figname:
        raise nodes.SkipNode
    targetdoc = self.builder.env.docnames_by_figname[target]

    ref_link = '%s:%s' % (targetdoc, target)

    if label is None:
        latex = '\\ref{%s}' % ref_link
    else:
        latex = "\\hyperref[%s]{%s \\ref*{%s}}" % (ref_link, label, ref_link)

    self.body.append(latex)
    raise nodes.SkipNode


def doctree_read(app, doctree):
    # first generate figure numbers for each figure
    env = app.builder.env

    docname_figs = getattr(env, 'docname_figs', {})
    docnames_by_figname = getattr(env, 'docnames_by_figname', {})

    for figure_info in doctree.traverse(lambda n: isinstance(n, nodes.figure) or \
                                                  isinstance(n, subfig.subfigend) or \
                                                  isinstance(n, figtable.figtable)):

        for id in figure_info['ids']:

            fig_docname = docnames_by_figname[id] = env.docname

            if fig_docname not in docname_figs:
                docname_figs[fig_docname] = OrderedDict()

            if isinstance(figure_info.parent, subfig.subfig):
                mainid = figure_info.parent['mainfigid']
            else:
                mainid = id

            if mainid not in docname_figs[fig_docname]:
                docname_figs[fig_docname][mainid] = OrderedSet()

            if isinstance(figure_info.parent, subfig.subfig):
                docname_figs[fig_docname][mainid].add(id)

    env.docnames_by_figname = docnames_by_figname
    env.docname_figs = docname_figs

def figname_is_id(s):
    "determine if string is 'idX' where X is an integer"
    if s.startswith('id') and len(s) > 2:
        isint = False
        try:
            isint = int(s[2:]) is not None
        except ValueError:
            isint = False
        return isint


def doctree_resolved(app, doctree, docname):
    # replace numfig nodes with links
    if app.builder.name in ('html', 'singlehtml', 'epub'):
        env = app.builder.env

        docname_figs = getattr(env, 'docname_figs', {})
        docnames_by_figname = env.docnames_by_figname

        figids = getattr(env, 'figids', {})

        secnums = []
        fignames_by_secnum = {}
        for figdocname, figurelist in env.docname_figs.items():
            if figdocname not in env.toc_secnumbers:
                continue
            secnum = env.toc_secnumbers[figdocname]['']
            secnums.append(secnum)
            fignames_by_secnum[secnum] = figurelist

        last_secnum = 0
        secnums = sorted(secnums)
        figid = 1
        for secnum in secnums:
            if secnum[0] != last_secnum:
                figid = 1
            for figname, subfigs in fignames_by_secnum[secnum].items():
                if figname_is_id(figname):
                    continue
                figids[figname] = str(secnum[0]) + '.' + str(figid)
                isub = 0
                for subfigname in subfigs:
                    subfigid = figids[figname] + chr(ord('a') + isub)
                    figids[subfigname] = subfigid
                    # if not figname_is_id(subfigname):
                    isub += 1
                figid += 1
            last_secnum = secnum[0]

            env.figids = figids

        for figure_info in doctree.traverse(lambda n: isinstance(n, nodes.figure) or \
                                                      isinstance(n, subfig.subfigend) or \
                                                      isinstance(n, figtable.figtable)):

            try:
                for i in range(len(figure_info['ids'])):
                    id = figure_info['ids'][i]
                    if id in figids:
                        break
                fignum = figids[id]
            except (IndexError, KeyError):
                continue

            for cap in figure_info.traverse(nodes.caption):
                cap.insert(1, nodes.Text(" %s" % cap[0]))
                if fignum[-1] in map(str, range(10)):
                    boldcaption = "%s %s:" % (app.config.figure_caption_prefix, fignum)
                else:
                    boldcaption = "(%s)" % fignum[-1]
                cap[0] = nodes.strong('', boldcaption)

        for ref_info in doctree.traverse(num_ref):
            if '#' in ref_info['reftarget']:
                label, target = ref_info['reftarget'].split('#')
                labelfmt = label + " %s"
            else:
                labelfmt = '%s'
                target = ref_info['reftarget']

            if target not in docnames_by_figname:
                print('\033[91m' + 'WARNING Target figure not found: %s' % target + '\033[0m')
                link = "#%s" % target
                linktext = target
            else:
                target_doc = docnames_by_figname[target]

                if app.builder.name == 'singlehtml':
                    link = "#%s" % target
                else:
                    link = "%s#%s"%(app.builder.get_relative_uri(docname,
                                                                 target_doc),
                                    target)
                if target in figids:
                    linktext = labelfmt % figids[target]
                else:
                    linktext = target


            html = '<a href="%s">%s</a>' % (link, linktext)
            ref_info.replace_self(nodes.raw(html, html, format='html'))

def setup(app):
    app.add_config_value('number_figures', True, True)
    app.add_config_value('figure_caption_prefix', "Figure", True)

    app.add_node(page_ref,
                 text=(skip_page_ref, None),
                 html=(skip_page_ref, None),
                 singlehtml=(skip_page_ref, None),
                 latex=(latex_visit_page_ref, None))

    app.add_role('page', XRefRole(nodeclass=page_ref))

    app.add_node(num_ref,
                 latex=(latex_visit_num_ref, None),
                 text=(skip_page_ref, None))

    app.add_role('num', XRefRole(nodeclass=num_ref))

    app.connect('doctree-read', doctree_read)
    app.connect('doctree-resolved', doctree_resolved)
