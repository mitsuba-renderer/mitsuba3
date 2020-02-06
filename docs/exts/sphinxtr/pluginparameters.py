
__docformat__ = 'reStructuredText'


import sys
import os.path
import csv

from docutils import io, nodes, statemachine, utils
from docutils.utils.error_reporting import SafeString
from docutils.utils import SystemMessagePropagation
from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives


class Table(Directive):

    """
    Generic table base class.
    """

    optional_arguments = 1
    final_argument_whitespace = True
    option_spec = {'class': directives.class_option,
                   'name': directives.unchanged}
    has_content = True

    def make_title(self):
        if self.arguments:
            title_text = self.arguments[0]
            text_nodes, messages = self.state.inline_text(title_text,
                                                          self.lineno)
            title = nodes.title(title_text, '', *text_nodes)
        else:
            title = None
            messages = []
        return title, messages

    def process_header_option(self):
        source = self.state_machine.get_source(self.lineno - 1)
        table_head = []
        max_header_cols = 0
        if 'header' in self.options:   # separate table header in option
            rows, max_header_cols = self.parse_csv_data_into_rows(
                self.options['header'].split('\n'), self.HeaderDialect(),
                source)
            table_head.extend(rows)
        return table_head, max_header_cols

    def check_table_dimensions(self, rows, header_rows, stub_columns):
        if len(rows) < header_rows:
            error = self.state_machine.reporter.error(
                '%s header row(s) specified but only %s row(s) of data '
                'supplied ("%s" directive).'
                % (header_rows, len(rows), self.name), nodes.literal_block(
                self.block_text, self.block_text), line=self.lineno)
            raise SystemMessagePropagation(error)
        if len(rows) + 1 == header_rows > 0:
            error = self.state_machine.reporter.error(
                'Insufficient data supplied (%s row(s)); no data remaining '
                'for table body, required by "%s" directive.'
                % (len(rows), self.name), nodes.literal_block(
                self.block_text, self.block_text), line=self.lineno)
            raise SystemMessagePropagation(error)
        for row in rows:
            if len(row) < stub_columns:
                error = self.state_machine.reporter.error(
                    '%s stub column(s) specified but only %s columns(s) of '
                    'data supplied ("%s" directive).' %
                    (stub_columns, len(row), self.name), nodes.literal_block(
                    self.block_text, self.block_text), line=self.lineno)
                raise SystemMessagePropagation(error)
            if len(row) == stub_columns > 0:
                error = self.state_machine.reporter.error(
                    'Insufficient data supplied (%s columns(s)); no data remaining '
                    'for table body, required by "%s" directive.'
                    % (len(row), self.name), nodes.literal_block(
                    self.block_text, self.block_text), line=self.lineno)
                raise SystemMessagePropagation(error)

    def extend_short_rows_with_empty_cells(self, columns, parts):
        for part in parts:
            for row in part:
                if len(row) < columns:
                    row.extend([(0, 0, 0, [])] * (columns - len(row)))


class CustomListTable(Table):

    """
    Implement tables whose data is encoded as a uniform two-level bullet list using to describe
    Mitsuba 2 plugin parameters.
    """

    option_spec = {}

    def run(self):
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
        col_widths = [20, 15, 65]

        table_data = [[item.children for item in row_list[0]]
                        for row_list in node[0]]
        header_rows = self.options.get('header-rows', 1)
        stub_columns = self.options.get('stub-columns', 0)
        self.check_table_dimensions(table_data, header_rows, stub_columns)

        table_node = self.build_table_from_list(table_data, col_widths,
                                                header_rows, stub_columns)
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
            elif item_index:
                # ATTN pychecker users: num_cols is guaranteed to be set in the
                # "else" clause below for item_index==0, before this branch is
                # triggered.
                if len(item[0]) != num_cols:
                    error = self.state_machine.reporter.error(
                        'Error parsing content block for the "%s" directive: '
                        'uniform two-level bullet list expected, but row %s '
                        'does not contain the same number of items as row 1 '
                        '(%s vs %s).'
                        % (self.name, item_index + 1, len(item[0]), num_cols),
                        nodes.literal_block(self.block_text, self.block_text),
                        line=self.lineno)
                    raise SystemMessagePropagation(error)
            else:
                num_cols = len(item[0])
        return num_cols

    def build_table_from_list(self, table_data, col_widths, header_rows, stub_columns):
        table = nodes.table()
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
        header_text = ['Parameter', 'Type', 'Description']
        header_row_node = nodes.row()
        for text in header_text:
            entry = nodes.entry()
            entry += [nodes.paragraph(text="Parameter")]
            header_row_node += entry
        rows.append(header_row_node)

        for row in table_data:
            row_node = nodes.row()
            for cell in row:
                entry = nodes.entry()
                entry += cell
                row_node += entry
            rows.append(row_node)
        if header_rows:
            thead = nodes.thead()
            thead.extend(rows[:header_rows])
            tgroup += thead
        tbody = nodes.tbody()
        tbody.extend(rows[header_rows:])
        tgroup += tbody
        return table

def setup(app):
    app.add_directive('pluginparameters', CustomListTable)