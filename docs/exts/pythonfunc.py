__docformat__ = 'reStructuredText'

from docutils import nodes, utils
from docutils.parsers.rst.roles import code_role, set_implicit_options, set_classes
from docutils.utils.code_analyzer import Lexer, LexerError
import re


matcher = re.compile(r"([^\\])(\{(.*?[^\\])\})")


def pythonfunc_role(role, rawtext, text, lineno, inliner, options={}, content=[]):
    matches = [m[2] for m in re.findall(matcher, text)]
    code_string, number_of_refs = re.subn(matcher, r"\g<1>\g<3>", text)

    # TODO: This does not work as intended for the general case:
    # The substitution is over all appearances of the identifier inside {...},
    # regardless if it is inside {...} or not
    # This can be fixed with some work, but it is enough for our use-case

    set_classes(options)
    language = 'Python'
    classes = ['code']
    if 'classes' in options:
        classes.extend(options['classes'])
    if language and language not in classes:
        classes.append(language)

    try:
        tokens = Lexer(utils.unescape(code_string, True), language,
                       inliner.document.settings.syntax_highlight)
    except LexerError as error:
        msg = inliner.reporter.warning(error)
        prb = inliner.problematic(rawtext, rawtext, msg)
        return [prb], [msg]

    node = nodes.literal(rawtext, '', classes=classes)

    # analyze content and add nodes for every token
    for classes, value in tokens:
        ref_node = node
        if value in matches:
            ref_node = nodes.reference(
                f":ref:`{value}`", '', refuri=f"#{value}")
            node += ref_node

        if classes:
            ref_node += nodes.inline(value, value, classes=classes)
        else:
            ref_node += nodes.Text(value, value)

    return [node], []


pythonfunc_role.options = code_role.options.copy()


def setup(app):
    set_implicit_options(pythonfunc_role)
    app.add_role('pythonfunc', pythonfunc_role)
