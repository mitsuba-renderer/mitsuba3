__docformat__ = 'reStructuredText'

import sys
import os.path
import csv

from docutils import nodes
from docutils.utils import SystemMessagePropagation
from docutils.parsers.rst import Directive, Parser
import docutils.parsers.rst.directives as directives
from docutils.parsers.rst.directives.tables import Table
from docutils.statemachine import ViewList
from sphinx.util.nodes import nested_parse_with_titles

class PluginParameters(Table):
    """
    Implement tables whose data is encoded as a uniform two-level bullet list
    using to describe Mitsuba 3 plugin parameters.
    """

    optional_arguments = 1
    option_spec = {
        'extra-rows': int
    }

    def run(self):

        extra_rows = self.options.get('extra-rows', 0)
        if not self.content:
            error = self.state_machine.reporter.error(
                'The "%s" directive is empty; content required.' % self.name,
                nodes.literal_block(self.block_text, self.block_text),
                line=self.lineno)
            return [error]
        title, messages = self.make_title()
        node = nodes.Element()          # anonymous container for parsing
        self.state.nested_parse(self.content, self.content_offset, node)

        num_cols = self.check_list_content(node)

        # Hardcode this:
        col_widths = [20, 12, 60, 8]

        table_data = [[item.children for item in row_list[0]]
                        for row_list in node[0]]
        header_rows = self.options.get('header-rows', 1)
        stub_columns = self.options.get('stub-columns', 0)
        self.check_table_dimensions(table_data, header_rows-1, stub_columns)

        table_node = self.build_table_from_list(table_data, col_widths,
                                                header_rows, extra_rows,
                                                stub_columns)
        table_node['classes'] += self.options.get('class', ['paramstable'])
        self.add_name(table_node)
        if title:
            table_node.insert(0, title)
        return [table_node] + messages

    def check_list_content(self, node):
        if len(node) != 1 or not isinstance(node[0], nodes.bullet_list):
            error = self.state_machine.reporter.error(
                'Error parsing content block for the "%s" directive: '
                'exactly one bullet list expected.' % self.name,
                nodes.literal_block(self.block_text, self.block_text),
                line=self.lineno)
            raise SystemMessagePropagation(error)
        list_node = node[0]
        # Check for a uniform two-level bullet list:
        for item_index in range(len(list_node)):
            item = list_node[item_index]
            if len(item) != 1 or not isinstance(item[0], nodes.bullet_list):
                error = self.state_machine.reporter.error(
                    'Error parsing content block for the "%s" directive: '
                    'two-level bullet list expected, but row %s does not '
                    'contain a second-level bullet list.'
                    % (self.name, item_index + 1), nodes.literal_block(
                    self.block_text, self.block_text), line=self.lineno)
                raise SystemMessagePropagation(error)
            else:
                num_cols = len(item[0])
        return 4

    def build_table_from_list(self, table_data, col_widths, header_rows, extra_rows, stub_columns):
        table = nodes.table(width='100%')
        tgroup = nodes.tgroup(cols=len(col_widths))
        table += tgroup
        for col_width in col_widths:
            colspec = nodes.colspec(colwidth=col_width)
            if stub_columns:
                colspec.attributes['stub'] = 1
                stub_columns -= 1
            tgroup += colspec
        rows = []

        # Append first row
        header_text = ['Parameter', 'Type', 'Description', 'Flags']
        header_row_node = nodes.row()
        for text in header_text:
            entry = nodes.entry()
            entry += [nodes.paragraph(text=text)]
            header_row_node += entry
        rows.append(header_row_node)

        for row in table_data:
            row_node = nodes.row()
            for i, cell in enumerate(row):
                entry = nodes.entry()

                # force the first column to be write in paramtype style
                if i == 0:
                    rst = ViewList()
                    rst.append(""":paramtype:`{name}`""".format(name=str(cell[0][0])), "", 0)
                    parsed_node = nodes.section()
                    parsed_node.document = self.state.document
                    nested_parse_with_titles(self.state, rst, parsed_node)

                    entry += [parsed_node[0]]
                else:
                    entry += cell

                row_node += entry

            # Add an empty cell if only 3 entries are provided
            if len(row) == 3:
                entry = nodes.entry()
                entry += [nodes.paragraph(text='')]
                row_node += entry

            rows.append(row_node)
        if header_rows:
            thead = nodes.thead()
            thead.extend(rows[:header_rows])
            tgroup += thead
        tbody = nodes.tbody()

        if extra_rows > 0:
            tbody.extend(rows[header_rows:extra_rows+1])
        else:
            tbody.extend(rows[header_rows:])
        tgroup += tbody

        # Handle the extra state header and rows
        if extra_rows > 0:
            # Append first row
            header_text = ['State parameters', '', '', '']
            header_row_node = nodes.row()
            for text in header_text:
                entry = nodes.entry()
                entry += [nodes.paragraph(text=text)]
                header_row_node += entry

            thead = nodes.thead()
            thead.extend([header_row_node])
            tgroup += thead

            tbody = nodes.tbody()
            tbody.extend(rows[extra_rows+1:])
            tgroup += tbody

        return table

def setup(app):
    app.add_directive('pluginparameters', PluginParameters)
