# -*- coding: utf-8 -*-
import os

from docutils.io import FileOutput
from docutils.frontend import OptionParser
from docutils import nodes

from docutils.utils import smartquotes

import sphinx.builders.latex
# from sphinx.util.smartypants import educate_quotes_latex
from sphinx.writers.latex import LaTeXWriter
from sphinx.util.console import bold
from sphinx.util.osutil import copyfile
from sphinx.util.texescape import tex_escape_map
import sphinx.writers.latex

# remove usepackage for sphinx here, we add it later in the preamble in conf.py
# sphinx.writers.latex.HEADER = sphinx.writers.latex.HEADER.replace('\usepackage{sphinx}', '')

import re


# Constants for quote education.
punct_class = r"""[!"#\$\%'()*+,-.\/:;<=>?\@\[\\\]\^_`{|}~]"""
end_of_word_class = r"""[\s.,;:!?)]"""
close_class = r"""[^\ \t\r\n\[\{\(\-]"""
dec_dashes = r"""&#8211;|&#8212;"""

# Special case if the very first character is a quote
# followed by punctuation at a non-word-break. Close the quotes by brute force:
single_quote_start_re = re.compile(r"""^'(?=%s\\B)""" % (punct_class,))
double_quote_start_re = re.compile(r"""^"(?=%s\\B)""" % (punct_class,))

# Special case for double sets of quotes, e.g.:
#   <p>He said, "'Quoted' words in a larger quote."</p>
double_quote_sets_re = re.compile(r""""'(?=\w)""")
single_quote_sets_re = re.compile(r"""'"(?=\w)""")

# Special case for decade abbreviations (the '80s):
decade_abbr_re = re.compile(r"""\b'(?=\d{2}s)""")

# Get most opening double quotes:
opening_double_quotes_regex = re.compile(r"""
                (
                        \s          |   # a whitespace char, or
                        &#160;      |   # a non-breaking space entity, or
                        --          |   # dashes, or
                        &[mn]dash;  |   # named dash entities
                        %s          |   # or decimal entities
                        &\#x201[34];    # or hex
                )
                "                 # the quote
                (?=\w)            # followed by a word character
                """ % (dec_dashes,), re.VERBOSE)

# Double closing quotes:
closing_double_quotes_regex = re.compile(r"""
                #(%s)?   # character that indicates the quote should be closing
                "
                (?=%s)
                """ % (close_class, end_of_word_class), re.VERBOSE)

closing_double_quotes_regex_2 = re.compile(r"""
                (%s)   # character that indicates the quote should be closing
                "
                """ % (close_class,), re.VERBOSE)

# Get most opening single quotes:
opening_single_quotes_regex = re.compile(r"""
                (
                        \s          |   # a whitespace char, or
                        &#160;      |   # a non-breaking space entity, or
                        --          |   # dashes, or
                        &[mn]dash;  |   # named dash entities
                        %s          |   # or decimal entities
                        &\#x201[34];    # or hex
                )
                '                 # the quote
                (?=\w)            # followed by a word character
                """ % (dec_dashes,), re.VERBOSE)

closing_single_quotes_regex = re.compile(r"""
                (%s)
                '
                (?!\s | s\b | \d)
                """ % (close_class,), re.VERBOSE)

closing_single_quotes_regex_2 = re.compile(r"""
                (%s)
                '
                (\s | s\b)
                """ % (close_class,), re.VERBOSE)


def educate_quotes_latex(s, dquotes=("``", "''")):
    """
    Parameter:  String.

    Returns:    The string, with double quotes corrected to LaTeX quotes.

    Example input:  "Isn't this fun?"
    Example output: ``Isn't this fun?'';
    """

    # Special case if the very first character is a quote
    # followed by punctuation at a non-word-break. Close the quotes
    # by brute force:
    s = single_quote_start_re.sub("\x04", s)
    s = double_quote_start_re.sub("\x02", s)

    # Special case for double sets of quotes, e.g.:
    #   <p>He said, "'Quoted' words in a larger quote."</p>
    s = double_quote_sets_re.sub("\x01\x03", s)
    s = single_quote_sets_re.sub("\x03\x01", s)

    # Special case for decade abbreviations (the '80s):
    s = decade_abbr_re.sub("\x04", s)

    s = opening_single_quotes_regex.sub("\\1\x03", s)
    s = closing_single_quotes_regex.sub("\\1\x04", s)
    s = closing_single_quotes_regex_2.sub("\\1\x04\\2", s)

    # Any remaining single quotes should be opening ones:
    s = s.replace("'", "\x03")

    s = opening_double_quotes_regex.sub("\\1\x01", s)
    s = closing_double_quotes_regex.sub("\x02", s)
    s = closing_double_quotes_regex_2.sub("\\1\x02", s)

    # Any remaining quotes should be opening ones.
    s = s.replace('"', "\x01")

    # Finally, replace all helpers with quotes.
    return s.replace("\x01", dquotes[0]).replace("\x02", dquotes[1]).\
        replace("\x03", "`").replace("\x04", "'")


BaseTranslator = sphinx.writers.latex.LaTeXTranslator

class DocTranslator(BaseTranslator):

    def __init__(self, *args, **kwargs):
        BaseTranslator.__init__(self, *args, **kwargs)
        self.verbatim = None
    def visit_caption(self, node):
        caption_idx = node.parent.index(node)
        if caption_idx > 0:
            look_node = node.parent.children[caption_idx - 1]
        else:
            look_node = node.parent

        short_caption = unicode(look_node.get('alt', '')).translate(tex_escape_map)
        if short_caption != "":
            short_caption = '[%s]' % short_caption

        self.in_caption += 1
        self.body.append('\\caption%s{' % short_caption)
    def depart_caption(self, node):
        self.body.append('}')
        self.in_caption -= 1

    def visit_Text(self, node):
        if self.verbatim is not None:
            self.verbatim += node.astext()
        else:
            text = self.encode(node.astext())
            if '\\textasciitilde{}' in text:
                text = text.replace('\\textasciitilde{}', '~')
            if not self.no_contractions:
                # text = smartquotes(text)
                text = educate_quotes_latex(text)
            self.body.append(text)

    def visit_table(self, node):
        if self.table:
            raise UnsupportedError(
                '%s:%s: nested tables are not yet implemented.' %
                (self.curfilestack[-1], node.line or ''))

        self.table = sphinx.writers.latex.Table()
        self.table.longtable = False
        self.tablebody = []
        self.tableheaders = []

        # Redirect body output until table is finished.
        self._body = self.body
        self.body = self.tablebody

    def depart_table(self, node):
        self.body = self._body
        colspec = self.table.colspec or ''

        if 'p{' in colspec or 'm{' in colspec or 'b{' in colspec:
            self.body.append('\n\\bodyspacing\n')

        self.body.append('\n\\begin{tabular}')

        if colspec:
            self.body.append(colspec)
        else:
            self.body.append('{|' + ('l|' * self.table.colcount) + '}')

        self.body.append('\n')

        if self.table.caption is not None:
            for id in self.next_table_ids:
                self.body.append(self.hypertarget(id, anchor=False))
            self.next_table_ids.clear()

        self.body.append('\\toprule\n')

        self.body.extend(self.tableheaders)

        self.body.append('\\midrule\n')

        self.body.extend(self.tablebody)

        self.body.append('\\bottomrule\n')

        self.body.append('\n\\end{tabular}\n')

        self.table = None
        self.tablebody = None

    def depart_row(self, node):
        if self.previous_spanning_row == 1:
            self.previous_spanning_row = 0
            self.body.append('\\\\\n')
        else:
            self.body.append('\\\\\n')
        self.table.rowcount += 1

    def depart_literal_block(self, node):
        code = self.verbatim.rstrip('\n')
        lang = self.hlsettingstack[-1][0]
        linenos = code.count('\n') >= self.hlsettingstack[-1][1] - 1
        highlight_args = node.get('highlight_args', {})
        if 'language' in node:
            # code-block directives
            lang = node['language']
            highlight_args['force'] = True
        if 'linenos' in node:
            linenos = node['linenos']
        def warner(msg):
            self.builder.warn(msg, (self.curfilestack[-1], node.line))
        hlcode = self.highlighter.highlight_block(code, lang, warn=warner,
                linenos=linenos, **highlight_args)
        hlcode = hlcode.replace('\$', '$')
        hlcode = hlcode.replace('\%', '%')
        # workaround for Unicode issue
        hlcode = hlcode.replace(u'€', u'@texteuro[]')
        # must use original Verbatim environment and "tabular" environment
        if self.table:
            hlcode = hlcode.replace('\\begin{Verbatim}',
                                    '\\begin{OriginalVerbatim}')
            self.table.has_problematic = True
            self.table.has_verbatim = True
        # get consistent trailer
        hlcode = hlcode.rstrip()[:-14] # strip \end{Verbatim}
        hlcode = hlcode.rstrip() + '\n'
        hlcode = '\n' + hlcode + '\\end{%sVerbatim}\n' % (self.table and 'Original' or '')
        hlcode = hlcode.replace('Verbatim', 'lstlisting')
        begin_bracket = hlcode.find('[')
        end_bracket = hlcode.find(']')
        hlcode = hlcode[:begin_bracket] + '[]' + hlcode[end_bracket+1:]
        self.body.append(hlcode)
        self.verbatim = None

    def visit_figure(self, node):
        ids = ''
        for id in self.next_figure_ids:
            ids += self.hypertarget(id, anchor=False)
        self.next_figure_ids.clear()
        if 'width' in node and node.get('align', '') in ('left', 'right'):
            self.body.append('\\begin{wrapfigure}{%s}{%s}\n\\centering' %
                             (node['align'] == 'right' and 'r' or 'l',
                              node['width']))
            self.context.append(ids + '\\end{wrapfigure}\n')
        else:
            if (not 'align' in node.attributes or
                node.attributes['align'] == 'center'):
                # centering does not add vertical space like center.
                align = '\n\\centering'
                align_end = ''
            else:
                # TODO non vertical space for other alignments.
                align = '\\begin{flush%s}' % node.attributes['align']
                align_end = '\\end{flush%s}' % node.attributes['align']
            self.body.append('\\begin{figure}[tbp]%s\n' % align)
            if any(isinstance(child, nodes.caption) for child in node):
                self.body.append('\\capstart\n')
            self.context.append(ids + align_end + '\\end{figure}\n')

# sphinx.writers.latex.LaTeXTranslator = DocTranslator

class CustomLaTeXTranslator(DocTranslator):
    def astext(self):
            return (#HEADER % self.elements +
                    #self.highlighter.get_stylesheet() +
                    u''.join(self.body)
                    #'\n' + self.elements['footer'] + '\n' +
                    #self.generate_indices() +
                    #FOOTER % self.elements
                    )

    def unknown_departure(self, node):
        if node.tagname == 'only':
            return
        return super(CustomLaTeXTranslator, self).unknown_departure(node)

    def unknown_visit(self, node):
        if node.tagname == 'only':
            return
        return super(CustomLaTeXTranslator, self).unknown_visit(node)

class CustomLaTeXBuilder(sphinx.builders.latex.LaTeXBuilder):

    def write(self, *ignored):
        super(CustomLaTeXBuilder, self).write(*ignored)

        backup_translator = sphinx.writers.latex.LaTeXTranslator
        sphinx.writers.latex.LaTeXTranslator = CustomLaTeXTranslator
        backup_doc = sphinx.writers.latex.BEGIN_DOC
        sphinx.writers.latex.BEGIN_DOC = ''

        # output these as include files
        for docname in ['abstract', 'dedication', 'acknowledgements']:
            destination = FileOutput(
                    destination_path=os.path.join(self.outdir, '%s.inc' % docname),
                    encoding='utf-8')

            docwriter = LaTeXWriter(self)
            doctree = self.env.get_doctree(docname)

            docsettings = OptionParser(
                defaults=self.env.settings,
                components=(docwriter,)).get_default_values()
            doctree.settings = docsettings
            docwriter.write(doctree, destination)

        sphinx.writers.latex.LaTeXTranslator = backup_translator
        sphinx.writers.latex.BEGIN_DOC = backup_doc

    def finish(self, *args, **kwargs):
        super(CustomLaTeXBuilder, self).finish(*args, **kwargs)
        # copy additional files again *after* tex support files so we can override them!
        if self.config.latex_additional_files:
            self.info(bold('copying additional files again...'), nonl=1)
            for filename in self.config.latex_additional_files:
                self.info(' '+filename, nonl=1)
                copyfile(os.path.join(self.confdir, filename),
                         os.path.join(self.outdir, os.path.basename(filename)))
            self.info()

# monkey patch the shit out of it
# sphinx.builders.latex.LaTeXBuilder = CustomLaTeXBuilder
