#!/usr/bin/env python3
# coding: utf-8

from utils import load_api
import inspect
import re

SUBSECTION = "-----------------------------------------------"
SUBSUBSECTION = "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"


def dir_class(c):
    for prop_name in dir(c):
        if prop_name.startswith("__"):
            continue

        try:
            data = getattr(c, prop_name)
        except:
            continue

        yield prop_name, data


def cleanup_doc(doc):
    # TODO: Better separate header and actual documentation stuff
    return doc.replace("ignis.pyignis.", "").replace(r"/)", ")").replace(", )", ")").strip()


single_newline = re.compile(r"([^\n])\n([^\n])")
ident_line = re.compile(r"\n([^\n])")
enum_entry_section = re.compile(r"^\s{2}(\w+)$", re.M)
identifier = re.compile(r"[\w_]+")


def sub_arrays(lines: str):
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(4, 4), order='F']", "Mat4x4")
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(2), order='C']", "Vec2")
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(3), order='C']", "Vec3")
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(4), order='C']", "Vec4")
    lines = lines.replace(
        "ndarray[dtype=uint32, shape=(*, *), order='C', device='cpu']", "CPUArray2d_UInt32")
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(*, *, 3)]", "Image")
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(*, *, 3), order='C', device='cpu']", "CPUImage")
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(*, 3)]", "list[Vec3]")
    lines = lines.replace(
        "numpy.ndarray[dtype=float32, shape=(*, 3)]", "list[Vec3]")
    lines = lines.replace(
        "Eigen::Matrix<float, 4, 1, 0, 4, 1>", "Vec4")
    lines = lines.replace(
        "Eigen::Matrix<float, 3, 1, 0, 3, 1>", "Vec3")
    lines = lines.replace(
        "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >", "str")
    lines = lines.replace(
        "std::unordered_map<str, int, std::hash<str >, std::equal_to<str >, std::allocator<std::pair<str const, int> > >", "dict[str, int]")
    lines = lines.replace(
        "std::unordered_map<str, float, std::hash<str >, std::equal_to<str >, std::allocator<std::pair<str const, float> > >", "dict[str, float]")
    lines = lines.replace(
        "std::unordered_map<str, str, std::hash<str >, std::equal_to<str >, std::allocator<std::pair<str const, str > > >", "dict[str, str]")
    lines = lines.replace(
        "std::unordered_map<str, Vec3, std::hash<str >, std::equal_to<str >, Eigen::aligned_allocator<std::pair<str const, Vec3 > > >", "dict[str, Vec3]")
    lines = lines.replace(
        "std::unordered_map<str, Vec4, std::hash<str >, std::equal_to<str >, Eigen::aligned_allocator<std::pair<str const, Vec4 > > >", "dict[str, Vec4]")
    lines = lines.replace("collections.abc.Sequence", "list")
    lines = lines.replace("IG::", "")
    lines = lines.replace("SceneObject::", "SceneObject.")
    lines = lines.replace("SceneProperty::", "SceneProperty.")
    lines = lines.replace("array([[1., 0., 0., 0.], [0., 1., 0., 0.], [0., 0., 1., 0.], [0., 0., 0., 1.]], dtype=float32))", "Mat4x4.Identity")
    lines = lines.replace("array([0., 0.], dtype=float32)", "Vec2(0)")
    lines = lines.replace("array([0., 0., 0.], dtype=float32)", "Vec3(0)")
    return lines


def prepare_lines(lines):
    '''Prepare lines such that single new lines are collapsed, but keep multiple variants on each line'''
    func_name = identifier.findall(lines)[0] if lines[0].isalpha() else None
    lines = re.sub(single_newline, r"\g<1> \g<2>", lines)
    if func_name is not None:
        lines = lines.replace(func_name, "\n" + func_name).strip()
    return sub_arrays(lines).splitlines()


def link_identifiers(line: str, identifiers, inside_doc=False):
    if inside_doc:
        for ident in identifiers:
            line = re.sub(r"\b(?<!`)"+re.escape(ident)+r"(?!`)",
                          r":ref:`"+re.escape(ident)+r"`", line)
    else:
        for ident in identifiers:
            line = re.sub(r"\b(?<!{)"+re.escape(ident)+r"(?!})",
                          r"{"+ident+r"}", line)
    return line


def ident_lines(lines, prefix=" "):
    return re.sub(ident_line, r"\n"+prefix+r"\g<1>", lines)


def escape_func(lines, identifiers, inside_doc=False):
    lines = prepare_lines(lines)
    lines = [link_identifiers(line, identifiers, inside_doc) for line in lines]
    return lines


def handle_func_doc(lines, identifiers):
    lines = escape_func(lines, identifiers)
    return "\n\n".join([f"- :pythonfunc:`{line.strip()}`" for line in lines])


def handle_class_doc(lines):
    if lines is None:
        return "- :pythonfunc:`Missing Documentation`\n"
    else:
        return re.sub(enum_entry_section, r"- :pythonfunc:`\g<1>`\n", lines).replace("Members:", f"Entries\n{SUBSUBSECTION}\n")


def get_class_names(root):
    classes = []
    todo = [("", root)]
    while len(todo) > 0:
        name, cur = todo.pop(0)
        for prop_name, data in dir_class(cur):
            if inspect.isclass(data):
                if cur != root:
                    classes.append(f"{name}.{prop_name}")
                    todo.append((f"{name}.{prop_name}", data))
                else:
                    classes.append(prop_name)
                    todo.append((prop_name, data))

    return classes


def generate_doc(root):
    identifiers = get_class_names(ignis)
    identifiers.sort(key=len, reverse=True)

    doc_str = "Python API\n"
    doc_str += "==========\n"
    doc_str += "\n"

    todo = [("Ignis (module)", root)]
    while len(todo) > 0:
        name, cur = todo.pop(0)

        ref_name = name.replace(".", "-")
        doc_str += f'.. _{ref_name}:\n\n{name}\n{SUBSECTION}\n\n{handle_class_doc(inspect.getdoc(cur))}\n\n'

        # Properties
        has_props = False
        for prop_name, data in dir_class(cur):
            if isinstance(data, property):
                inner_doc = cleanup_doc(inspect.getdoc(data))

                if not has_props:
                    doc_str += f"Properties\n{SUBSUBSECTION}\n\n"
                    has_props = True

                lines = escape_func(inner_doc, identifiers, inside_doc=False)
                doc_str += "".join(
                    [f'- :pythonfunc:`{prop_name}: {line}`\n\n' for line in lines])

        # Methods
        has_funcs = False
        for prop_name, data in dir_class(cur):
            if inspect.isroutine(data):
                inner_doc = cleanup_doc(inspect.getdoc(data))
                if inner_doc == "":
                    continue

                if not has_funcs:
                    doc_str += f"Methods\n{SUBSUBSECTION}\n\n"
                    has_funcs = True
                doc_str += f'{handle_func_doc(inner_doc, identifiers)}\n\n'

        # Embedded classes
        for prop_name, data in dir_class(cur):
            if inspect.isclass(data):
                if cur != root:
                    todo.append((f"{name}.{prop_name}", data))
                else:
                    todo.append((prop_name, data))

        doc_str += "\n"

    return doc_str.rstrip() + "\n"


if __name__ == "__main__":
    ignis = load_api()
    print(generate_doc(ignis))
