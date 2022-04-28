__docformat__ = 'reStructuredText'

import sys
import os.path
import csv

from docutils import nodes
from docutils.utils import SystemMessagePropagation
from docutils.parsers.rst import Directive, Parser
from docutils.parsers.rst.directives.tables import Table
from docutils.statemachine import ViewList
from sphinx.util.nodes import nested_parse_with_titles

class ObjectParameters(Table):
    """
    Implement tables whose data is encoded as a uniform two-level bullet list using to describe
    Ignis scene object parameters.
    This essentially the same file as used in Mitsuba 2 documentation but extended to four columns
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

        # Hardcode this:
        col_widths = [20, 15, 15, 65]

        table_data = [[item.children for item in row_list[0]]
                        for row_list in node[0]]
        header_rows = self.options.get('header-rows', 1)
        stub_columns = self.options.get('stub-columns', 0)
        self.check_table_dimensions(table_data, header_rows-1, stub_columns)

        table_node = self.build_table_from_list(table_data, col_widths,
                                                header_rows, stub_columns)
        table_node['classes'] += self.options.get('class', ['paramstable'])
        self.add_name(table_node)
        if title:
            table_node.insert(0, title)
        return [table_node] + messages

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
        header_text = ['Parameter', 'Type', 'Default', 'Description']
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
    app.add_directive('objectparameters', ObjectParameters)