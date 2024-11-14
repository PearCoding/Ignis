__docformat__ = 'reStructuredText'

from docutils import nodes, utils
from docutils.parsers.rst.roles import code_role, set_implicit_options, set_classes
from docutils.utils.code_analyzer import Lexer, LexerError
import re


matcher = re.compile(r"(\{(.*?[^\\])\})")

def mergePoint(tokens):
        """Merge subsequent tokens via '.'"""
        tokens = iter(tokens)
        (lasttype, lastval) = next(tokens)
        combine = False
        for ttype, value in tokens:
            if value == '.':
                lastval += value
                pttype = ttype
                combine = True
            elif combine:
                lastval += value
                combine = False
            else:
                yield(lasttype, lastval)
                (lasttype, lastval) = (ttype, value)
        if lastval.endswith('\n'):
            lastval = lastval[:-1]
        if combine:
            lastval = lastval[:-1]
            yield(lasttype, lastval)
            (lasttype, lastval) = (pttype, '.')
        if lastval:
            yield(lasttype, lastval)


def pythonfunc_role(role, rawtext, text, lineno, inliner, options={}, content=[]):
    matches = [m[1] for m in re.findall(matcher, text)]
    code_string, number_of_refs = re.subn(matcher, r"\g<2>", text)

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
        tokens = Lexer(utils.unescape(code_string, True, True), language,
                       inliner.document.settings.syntax_highlight)
    except LexerError as error:
        msg = inliner.reporter.warning(error)
        prb = inliner.problematic(rawtext, rawtext, msg)
        return [prb], [msg]

    node = nodes.literal(rawtext, '', classes=classes)

    # analyze content and add nodes for every token
    for classes, value in mergePoint(tokens):
        ref_node = node
        if value in matches:
            ref_node = nodes.reference(
                f":ref:`{value}`", '', refid=value.replace('.', '-').lower())
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
