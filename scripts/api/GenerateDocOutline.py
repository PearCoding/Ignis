#!/usr/bin/env python3
# coding: utf-8

from utils import load_api
import inspect
import enum
import re


def get_doc(m: object):
    doc = inspect.getdoc(m)
    return inspect.cleandoc(doc) if doc is not None else ""


def split_methods(lines: str):
    '''Split line of methods such that parameters using newlines are grouped together. This only works if the newline starts with a non-alpha character (e.g., '[')'''
    it = iter(lines.splitlines())
    prev_line = next(it, None)
    for line in it:
        if line[0].isalpha():
            yield prev_line
            prev_line = line
        else:
            prev_line += line

    if prev_line:
        yield prev_line


def inspect_enum(e: enum.EnumType):
    return {
        "doc": get_doc(e),
        "members": list(e.__members__.keys())
    }


def inspect_property(p: property):
    doc = get_doc(p)
    signature = doc if doc.startswith("(self)") else ""
    doc = "" if doc.startswith("(self)") else doc

    return {
        "doc": doc,
        "type": signature.replace("(self) ->", "").strip()
    }


def inspect_method(m: object):
    doc = get_doc(m)
    components = doc.split("\n\n")
    overloads = list(split_methods(components[0]))
    info = components[1:]
    if len(overloads) == 1:
        return [{
                "doc": "\n".join(info),
                "signature": overloads[0]
            }]
    else:
        signatures = []
        for o in overloads:
            doc_line = ""
            for i, line in enumerate(info):
                if o in line:
                    doc_line = info[i+1]
            signatures.append({
                "doc": doc_line,
                "signature": o
            })

        return signatures


def inspect_module(m: object):
    classes = {}
    methods = {}
    properties = {}
    enums = {}
    enum_members = [] # Inline enums

    for name in dir(m):
        if name.startswith("__"):
            continue
        
        try:
            obj = getattr(m, name)
        except:
            continue
        
        if isinstance(obj, enum.EnumType):
            enums[name] = inspect_enum(obj)
        elif isinstance(obj, enum.Enum):
            enum_members.append(name)
        elif inspect.isclass(obj) or inspect.ismodule(obj):
            classes[name] = inspect_module(obj)
        elif isinstance(obj, property) or inspect.isgetsetdescriptor(obj):
            properties[name] = inspect_property(obj)
        else:
            methods[name] = inspect_method(obj)

    return {
        "doc": get_doc(m),
        "classes": classes,
        "methods": methods,
        "properties": properties,
        "enums": enums,
        "enum_members": enum_members
    }


def cleanup_line(line: str):
    line = line.replace("ignis.pyignis.", "")                # Get rid of the prefix
    line = line.replace("IG::", "")                          # Somehow this got inserted into the documentation
    line = line.replace("SceneObject::", "SceneObject.")     # Python uses . instead of ::
    line = line.replace("SceneProperty::", "SceneProperty.") #
    line = line.replace("collections.abc.Sequence", "list")  # Just a basic list
    line = line.replace("numpy.ndarray", "ndarray")          # Get rid of the (optional) numpy prefix
    line = line.replace("numpy.array", "array")              #
    line = re.sub(r"ndarray\[dtype=float32\s*,\s*shape=\((\d+)\s*,\s*(\d+)\)\s*,\s*order='F'\]", r"Mat\1x\2", line)                                 # Extract numpy matrix 
    line = re.sub(r"ndarray\[dtype=float32\s*,\s*shape=\((\d+)\)\s*,\s*order='C'\]", r"Vec\1", line)                                                # Extract numpy vector   
    line = re.sub(r"ndarray\[dtype=uint32\s*,\s*shape=\(\s*\*\s*,\s*\*\s*\)\s*,\s*order='C'\s*,\s*device='cpu'\s*\]", r"CPUArray2d_UInt32", line)   # Extract specialized array
    line = re.sub(r"ndarray\[dtype=float32\s*,\s*shape=\(\s*\*\s*,\s*\*\s*\)\s*,\s*order='C'\s*,\s*device='cpu'\s*\]", r"CPUArray2d_Float32", line) # 
    line = re.sub(r"ndarray\[dtype=float32\s*,\s*shape=\(\s*\*\s*,\s*\*\s*,\s*3\)\s*\]", r"Image", line)                                            # Extract image (cpu or gpu)
    line = re.sub(r"ndarray\[dtype=float32\s*,\s*shape=\(\s*\*\s*,\s*\*\s*,\s*3\)\s*,\s*order='C'\s*,\s*device='cpu'\s*\]", r"CPUImage", line)      # Extract image on the cpu
    line = re.sub(r"ndarray\[dtype=float32\s*,\s*shape=\(\s*\*\s*,\s*(\d+)\)\s*\]", r"list[Vec\1]", line)                                           # Extract a list of vectors
    line = re.sub(r"Eigen::Matrix<\s*float\s*,\s*(\d+)\s*,\s*1\s*,\s*0\s*,\s*\d+\s*,\s*1\s*>", r"Vec\1", line)                                      # Extract Eigen vectors
    line = re.sub(r"Eigen::Matrix<\s*float\s*,\s*(\d+)\s*,\s*1\s*,\s*0\s*,\s*\d+\s*,\s*1\s*>", r"Vec\1", line)                                      #
    line = re.sub(r"std::(__cxx11::)?basic_string<\s*char\s*,\s*std::char_traits<\s*char\s*>\s*,\s*std::allocator<\s*char\s*>\s*>", "str", line)                                                                               # Extract string
    line = re.sub(r"std::unordered_map<\s*str\s*,\s*(\w+)\s*,\s*std::hash<\s*str\s*>\s*,\s*std::equal_to<\s*str\s*>\s*,\s*std::allocator<\s*std::pair<\s*str\s+const\s*,\s*\w+\s*>\s*>\s*>", r"dict[str, \1]", line)           # Extract dictionary
    line = re.sub(r"std::unordered_map<\s*str\s*,\s*(\w+)\s*,\s*std::hash<\s*str\s*>\s*,\s*std::equal_to<\s*str\s*>\s*,\s*Eigen::aligned_allocator<\s*std::pair<\s*str\s+const\s*,\s*\w+\s*>\s*>\s*>", r"dict[str, \1]", line) #
    line = line.replace("array([[1., 0., 0., 0.],[0., 1., 0., 0.],[0., 0., 1., 0.],[0., 0., 0., 1.]], dtype=float32))", "Mat4x4.Identity") # Extract identity matrix
    line = line.replace("array([0., 0.], dtype=float32)", "Vec2(0)")                                                                       # Extract zero vector
    line = line.replace("array([0., 0., 0.], dtype=float32)", "Vec3(0)")                                                                   #
    line = line.replace(", /)", ")") # Remove weird trailing comma
    line = line.replace("/)", ")")   # Remove / at the end
    return line


def cleanup_module(info: dict):
    for c in info['classes'].values():
        cleanup_module(c)
    for m in info['methods'].values():
        for e in m:
            e['doc']       = cleanup_line(e['doc'])
            e['signature'] = cleanup_line(e['signature'])
    for p in info['properties'].values():
        p['doc']  = cleanup_line(p['doc'])
        p['type'] = cleanup_line(p['type'])
    
    info['doc'] = cleanup_line(info['doc'])
    return info


def get_list_of_identifiers(info: dict, prefix="") -> list:
    classes = list([prefix + key for key in info["classes"].keys()])
    classes.extend([prefix + key for key in info["enums"].keys()])
    for n, c in info["classes"].items():
        classes.extend(get_list_of_identifiers(c, prefix=prefix+n+"."))
    return classes


def link_identifiers_doc(line: str, identifiers: list):
    for ident in identifiers:
        line = re.sub(r"\b(?<!`)"+re.escape(ident)+r"(?!`)", ident+r"_", line)
    return line


def link_identifiers_code(line: str, identifiers: list):
    for ident in identifiers:
        line = re.sub(r"\b(?<!{)"+re.escape(ident)+r"(?!})", r"{"+ident+r"}", line)
    return line


def handle_identifiers(name: str, info: dict, identifiers: list):
    for n, c in info['classes'].items():
        handle_identifiers(n, c, identifiers)
    for m in info['methods'].values():
        for e in m:
            e['doc']       = link_identifiers_doc(e['doc'], identifiers)
            e['signature'] = link_identifiers_code(e['signature'], identifiers)
    for p in info['properties'].values():
        p['doc']  = link_identifiers_doc(p['doc'], identifiers)
        p['type'] = link_identifiers_code(p['type'], identifiers)
    
    info['doc'] = link_identifiers_doc(info['doc'], [id for id in identifiers if id != name])
    return info


ROOTNAME = "Ignis (module)"
SUBSECTION = "-----------------------------------------------"
SUBSUBSECTION = "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"

def generate_doc_module(name: str, info: dict) -> str:
    rst = ""

    ref_name = name.replace(".", "-")
    rst += f'.. _{ref_name}:\n\n{name}\n{SUBSECTION}\n\n'

    if len(info["doc"]) > 0:
        rst += info["doc"] + "\n\n"

    if len(info["enums"]) > 0:
        rst += f".. _{ref_name}-enums:\n\n"
        rst += f"Enums\n{SUBSUBSECTION}\n\n"
        for n, e in info["enums"].items():
            if ref_name != ROOTNAME:
                rst += f".. _{ref_name}-{n}:\n\n"
            else:
                rst += f".. _{n}:\n\n"
            rst += f"- :pythonfunc:`{n}`: [{', '.join(e['members'])}]\n\n"
        rst += "\n\n"

    if len(info["properties"]) > 0:
        rst += f".. _{ref_name}-properties:\n\n"
        rst += f"Properties\n{SUBSUBSECTION}\n\n"
        for n, p in info["properties"].items():
            if len(p['doc']) > 0:
                suffix = p['doc']
            else:
                suffix = f"Returns :pythonfunc:`{p['type']}` "
            rst += f"- :pythonfunc:`{n}`: {suffix}\n"
        rst += "\n\n"

    if len(info["methods"]) > 0:
        rst += f".. _{ref_name}-methods:\n\n"
        rst += f"Methods\n{SUBSUBSECTION}\n\n"
        for n, m in info["methods"].items():
            for sig in m:
                doc = sig['doc'] or '*No documentation*'
                rst += f"- :pythonfunc:`{sig['signature']}`: {doc}\n"
        rst += "\n\n"

    return (rst, list(info['classes'].items()))


def generate_doc(root: object):
    module_info = cleanup_module(inspect_module(root))
    identifiers = get_list_of_identifiers(module_info)
    identifiers.sort(key=len, reverse=True)

    module_info = handle_identifiers("", module_info, identifiers)

    rst = "Python API\n==========\n\n"

    stack = [(ROOTNAME, module_info)]
    while len(stack) > 0:
        name, info = stack.pop(0)
        child_markdown, inner = generate_doc_module(name, info)

        rst += child_markdown
        stack.extend(inner)
        stack.sort(key=lambda x: x[0])

    return rst


if __name__ == "__main__":
    ignis = load_api()
    print(generate_doc(ignis))
