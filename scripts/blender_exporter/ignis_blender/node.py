import bpy
import os

from .utils import *


TEXCOORD_UV = "uvw"


class NodeContext:
    def __init__(self, result, path, depsgraph, always_copy_images):
        self.result = result
        self.path = path
        self.depsgraph = depsgraph
        self.always_copy_images = always_copy_images
        self.cache = {}

        self._stack = []
        self._cur = 0

    def push(self, node):
        self._stack.insert(self._cur, node)
        self._cur += 1

    def pop(self):
        del self._stack[self._cur-1]
        self._cur -= 1

    @property
    def current(self):
        return self._stack[self._cur-1]

    def goIn(self):
        self._cur -= 1

    def goOut(self):
        self._cur += 1

    @property
    def scene(self):
        return self.depsgraph.scene


def _export_default(socket):
    default_value = getattr(socket, "default_value")
    if default_value is None:
        print(f"Socket {socket.name} has no default value")
        if socket.type == 'VECTOR':
            return "vec3(0)"
        elif socket.type == 'RGBA':
            return "color(0)"
        else:
            return 0
    else:
        if socket.type == 'VECTOR':
            return f"vec3({default_value[0]}, {default_value[1]}, {default_value[2]})"
        elif socket.type == 'RGBA':
            return f"color({default_value[0]}, {default_value[1]}, {default_value[2]}, {default_value[3]})"
        else:
            return default_value


def _export_scalar_value(ctx, node):
    return node.outputs[0].default_value


def _export_scalar_clamp(ctx, node):
    val = export_node(ctx, node.inputs[0])
    minv = export_node(ctx, node.inputs[1])
    maxv = export_node(ctx, node.inputs[2])

    return f"clamp({val}, {minv}, {maxv})"


def _export_maprange(ctx, node):
    val = export_node(ctx, node.inputs[0])
    from_min = export_node(ctx, node.inputs[1])
    from_max = export_node(ctx, node.inputs[2])
    to_min = export_node(ctx, node.inputs[3])
    to_max = export_node(ctx, node.inputs[4])

    ops = ""
    from_range = f"{from_max} - {from_min})"
    to_range = f"{to_max} - {to_min})"
    to_unit = f"({val} - {from_min}) / ({from_range})"
    if node.interpolation_type == "LINEAR":
        interp = to_unit
    elif node.interpolation_type == "SMOOTHSTEP":
        interp = f"smoothstep({to_unit})"
    elif node.interpolation_type == "SMOOTHERSTEP":
        interp = f"smootherstep({to_unit})"
    else:
        print(
            f"Not supported interpolation type {node.interpolation_type} for node {node.name}")
        return 0

    ops = f"((({interp}) * {to_range}) + {to_min})"
    if node.clamp:
        return f"clamp({ops}, {to_min}, {to_max})"
    else:
        return ops


def _export_fresnel(ctx, node):
    ior = export_node(ctx, node.inputs["IOR"])
    if node.inputs["Normal"].is_linked:
        normal = export_node(ctx, node.inputs["Normal"])
    else:
        normal = 'N'

    return f"fresnel_dielectric(select(frontside, 1/max(1e-5,{ior}), {ior}), dot({normal},V))"


def _export_layer_weight(ctx, node, output_name):
    blend = export_node(ctx, node.inputs["Blend"])
    if node.inputs["Normal"].is_linked:
        normal = export_node(ctx, node.inputs["Normal"])
    else:
        normal = 'N'

    cos_i = f"dot({normal},V)"

    if output_name == "Fresnel":
        eta = f"select(frontside, 1-{blend}, 1/max(1e-5,1-{blend}))"
        return f"fresnel_dielectric({eta}, {cos_i})"
    else:
        if try_extract_node_value(blend) == 0.5:
            return f"(1-abs({cos_i}))"
        else:
            blend_p = f"select({blend} < 0.5, 2*{blend}, 0.5/max(1e-5,1-{blend}))"
            return f"(1-abs({cos_i})^{blend_p})"


def _export_scalar_math(ctx, node):
    ops = ""
    if node.operation == "ADD":
        ops = f"({export_node(ctx, node.inputs[0])} + {export_node(ctx, node.inputs[1])})"
    elif node.operation == "SUBTRACT":
        ops = f"({export_node(ctx, node.inputs[0])} - {export_node(ctx, node.inputs[1])})"
    elif node.operation == "MULTIPLY":
        ops = f"({export_node(ctx, node.inputs[0])} * {export_node(ctx, node.inputs[1])})"
    elif node.operation == "DIVIDE":
        ops = f"({export_node(ctx, node.inputs[0])} / {export_node(ctx, node.inputs[1])})"
    elif node.operation == "MULTIPLY_ADD":
        ops = f"(({export_node(ctx, node.inputs[0])} * {export_node(ctx, node.inputs[1])}) + {export_node(ctx, node.inputs[2])})"
    elif node.operation == "POWER":
        ops = f"(({export_node(ctx, node.inputs[0])})^({export_node(ctx, node.inputs[1])}))"
    elif node.operation == "LOGARITHM":
        ops = f"log({export_node(ctx, node.inputs[0])})"
    elif node.operation == "SQRT":
        ops = f"sqrt({export_node(ctx, node.inputs[0])})"
    elif node.operation == "INVERSE_SQRT":
        ops = f"(1/sqrt({export_node(ctx, node.inputs[0])}))"
    elif node.operation == "ABSOLUTE":
        ops = f"abs({export_node(ctx, node.inputs[0])})"
    elif node.operation == "EXPONENT":
        ops = f"exp({export_node(ctx, node.inputs[0])})"
    elif node.operation == "MINIMUM":
        ops = f"min({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "MAXIMUM":
        ops = f"max({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "LESS_THAN":
        ops = f"select({export_node(ctx, node.inputs[0])} < {export_node(ctx, node.inputs[1])}, 1, 0)"
    elif node.operation == "GREATER_THAN":
        ops = f"select({export_node(ctx, node.inputs[0])} > {export_node(ctx, node.inputs[1])}, 1, 0)"
    elif node.operation == "SIGN":
        # TODO: If value is zero, zero should be returned!
        ops = f"select({export_node(ctx, node.inputs[0])} < 0, -1, 1)"
    elif node.operation == "COMPARE":
        ops = f"select(abs({export_node(ctx, node.inputs[0])} - {export_node(ctx, node.inputs[1])}) <= Eps, 1, 0)"
    elif node.operation == "ROUND":
        ops = f"round({export_node(ctx, node.inputs[0])})"
    elif node.operation == "FLOOR":
        ops = f"floor({export_node(ctx, node.inputs[0])})"
    elif node.operation == "CEIL":
        ops = f"ceil({export_node(ctx, node.inputs[0])})"
    elif node.operation == "TRUNC":
        ops = f"trunc({export_node(ctx, node.inputs[0])})"
    elif node.operation == "FRACTION":
        ops = f"fract({export_node(ctx, node.inputs[0])})"
    elif node.operation == "MODULO":
        ops = f"fmod({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "SMOOTH_MIN":
        ops = f"smin({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])}, {export_node(ctx, node.inputs[2])})"
    elif node.operation == "SMOOTH_MAX":
        ops = f"smax({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])}, {export_node(ctx, node.inputs[2])})"
    elif node.operation == "WRAP":
        ops = f"wrap({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])}, {export_node(ctx, node.inputs[2])})"
    elif node.operation == "SNAP":
        ops = f"snap({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "PINGPONG":
        ops = f"pingpong({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "SINE":
        ops = f"sin({export_node(ctx, node.inputs[0])})"
    elif node.operation == "COSINE":
        ops = f"cos({export_node(ctx, node.inputs[0])})"
    elif node.operation == "TANGENT":
        ops = f"tan({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCSINE":
        ops = f"asin({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCCOSINE":
        ops = f"acos({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCTANGENT":
        ops = f"atan({export_node(ctx, node.inputs[0])})"
    elif node.operation == "SINH":
        ops = f"sinh({export_node(ctx, node.inputs[0])})"
    elif node.operation == "COSH":
        ops = f"cosh({export_node(ctx, node.inputs[0])})"
    elif node.operation == "TANH":
        ops = f"tanh({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCTAN2":
        ops = f"atan2({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "RADIANS":
        ops = f"({export_node(ctx, node.inputs[0])} * Pi / 180)"
    elif node.operation == "DEGREES":
        ops = f"({export_node(ctx, node.inputs[0])} * 180 / Pi)"
    else:
        print(
            f"Not supported math operation type {node.operation} for node {node.name}")
        return 0

    if node.use_clamp:
        return f"clamp({ops}, 0, 1)"
    else:
        return ops


def _export_rgb_value(ctx, node):
    default_value = node.outputs[0].default_value
    return f"color({default_value[0]}, {default_value[1]}, {default_value[2]}, {default_value[3]})"


def _export_rgb_math(ctx, node):
    # See https://docs.gimp.org/en/gimp-concepts-layer-modes.html

    fac = export_node(ctx, node.inputs[0])
    col1 = export_node(ctx, node.inputs[1])  # Background (I)
    col2 = export_node(ctx, node.inputs[2])  # Foreground (M)

    ops = ""
    if node.blend_type == "MIX":
        ops = f"mix({col1}, {col2}, {fac})"
    elif node.blend_type == "BURN":
        ops = f"mix_burn({col1}, {col2}, {fac})"
    elif node.blend_type == "DARKEN":
        ops = f"mix({col1}, min({col1}, {col2}), {fac})"
    elif node.blend_type == "LIGHTEN":
        ops = f"mix({col1}, max({col1}, {col2}), {fac})"
    elif node.blend_type == "SCREEN":
        ops = f"mix_screen({col1}, {col2}, {fac})"
    elif node.blend_type == "DODGE":
        ops = f"mix_dodge({col1}, {col2}, {fac})"
    elif node.blend_type == "OVERLAY":
        ops = f"mix_overlay({col1}, {col2}, {fac})"
    elif node.blend_type == "SOFT_LIGHT":
        ops = f"mix_soft({col1}, {col2}, {fac})"
    elif node.blend_type == "LINEAR_LIGHT":
        ops = f"mix_linear({col1}, {col2}, {fac})"
    elif node.blend_type == "HUE":
        ops = f"mix_hue({col1}, {col2}, {fac})"
    elif node.blend_type == "SATURATION":
        ops = f"mix_saturation({col1}, {col2}, {fac})"
    elif node.blend_type == "VALUE":
        ops = f"mix_value({col1}, {col2}, {fac})"
    elif node.blend_type == "COLOR":
        ops = f"mix_color({col1}, {col2}, {fac})"
    elif node.blend_type == "DIFFERENCE":
        ops = f"mix({col1}, abs({col1} - {col2}), {fac})"
    elif node.blend_type == "ADD":
        ops = f"mix({col1}, {col1} + {col2}, {fac})"
    elif node.blend_type == "SUBTRACT":
        ops = f"mix({col1}, {col1} - {col2}, {fac})"
    elif node.blend_type == "MULTIPLY":
        ops = f"mix({col1}, {col1} * {col2}, {fac})"
    elif node.blend_type == "DIVIDE":
        ops = f"mix({col1}, {col1} / ({col2} + color(1)), {fac})"
    else:
        print(
            f"Not supported rgb math operation type {node.operation} for node {node.name}")
        return "color(0)"

    if node.use_clamp:
        return f"clamp({ops}, color(0), color(1))"
    else:
        return ops


def _export_rgb_gamma(ctx, node):
    color_node = export_node(ctx, node.inputs[0])
    gamma_node = export_node(ctx, node.inputs[1])
    return f"(({color_node})^({gamma_node}))"


def _export_rgb_brightcontrast(ctx, node):
    color = export_node(ctx, node.inputs["Color"])
    bright = export_node(ctx, node.inputs["Bright"])
    contrast = export_node(ctx, node.inputs["Contrast"])

    return f"max(color(0), (1+{contrast})*{color} + color({bright}-{contrast}*0.5))"


def _export_rgb_invert(ctx, node):
    # Only valid if values between 0 and 1

    fac = export_node(ctx, node.inputs[0])
    col1 = export_node(ctx, node.inputs[1])

    ops = f"(color(1) - {col1})"

    if node.inputs[0].is_linked or node.inputs[0].default_value != 1:
        return f"mix({col1}, {ops}, {fac})"
    else:
        return ops


def _export_combine_hsv(ctx, node):
    hue = export_node(ctx, node.inputs["H"])
    sat = export_node(ctx, node.inputs["S"])
    val = export_node(ctx, node.inputs["V"])

    return f"hsvtorgb(color({hue}, {sat}, {val}))"


def _export_combine_rgb(ctx, node):
    r = export_node(ctx, node.inputs["R"])
    g = export_node(ctx, node.inputs["G"])
    b = export_node(ctx, node.inputs["B"])

    return f"color({r}, {g}, {b})"


def _export_combine_xyz(ctx, node):
    x = export_node(ctx, node.inputs["X"])
    y = export_node(ctx, node.inputs["Y"])
    z = export_node(ctx, node.inputs["Z"])

    return f"vec3({x}, {y}, {z})"


def _export_separate_hsv(ctx, node, output_name):
    color = export_node(ctx, node.inputs[0])
    ops = f"rgbtohsv({color})"
    if output_name == "H":
        return f"{ops}.r"
    elif output_name == "S":
        return f"{ops}.g"
    else:
        return f"{ops}.b"


def _export_separate_rgb(ctx, node, output_name):
    color = export_node(ctx, node.inputs[0])
    if output_name == "R":
        return f"{color}.r"
    elif output_name == "G":
        return f"{color}.g"
    else:
        return f"{color}.b"


def _export_separate_xyz(ctx, node, output_name):
    vec = export_node(ctx, node.inputs[0])
    if output_name == "X":
        return f"{vec}.x"
    elif output_name == "Y":
        return f"{vec}.y"
    else:
        return f"{vec}.z"


def _export_hsv(ctx, node):
    hue = export_node(ctx, node.inputs[0])
    sat = export_node(ctx, node.inputs[1])
    val = export_node(ctx, node.inputs[2])
    fac = export_node(ctx, node.inputs[3])
    col = export_node(ctx, node.inputs[4])

    main = f"hsvtorgb(color(fmod(rgbtohsv({col}).x + {hue} + 0.5, 1), clamp(rgbtohsv({col}).y * {sat}, 0, 1), rgbtohsv({col}).z * {val}))"
    e_f = try_extract_node_value(fac, default=-1)

    if e_f == 1:
        ret = main
    elif e_f == 0:
        ret = col
    else:
        ret = f"mix({col}, {main}, {fac})"

    return f"max(color(0), {ret})"


def _export_blackbody(ctx, node):
    temp = export_node(ctx, node.inputs[0])
    return f"blackbody({temp})"


def _export_val_to_rgb(ctx, node):
    t = export_node(ctx, node.inputs[0])
    cr = node.color_ramp

    # Check if it is just the default one which we can skip
    if cr.interpolation == "LINEAR" and len(cr.elements) == 2:
        if cr.elements[0].position == 0 and cr.elements[0].color[0] == 0 and cr.elements[0].color[1] == 0 and cr.elements[0].color[2] == 0 and cr.elements[0].color[3] == 1 \
                and cr.elements[1].position == 1 and cr.elements[1].color[0] == 1 and cr.elements[1].color[1] == 1 and cr.elements[1].color[2] == 1 and cr.elements[1].color[3] == 1:
            return f"color({t})"

    # TODO: Add more interpolation methods
    # TODO: Add hue_interpolation
    if cr.interpolation == "CONSTANT":
        lookup_type = "'constant'"
    else:
        lookup_type = "'linear'"

    def check_constant(idx):
        a = cr.elements[0].color[idx]
        for p in cr.elements:
            if p.color[idx] != a:
                return False
        return True

    def gen_c(idx):
        if check_constant(idx):
            return cr.elements[0].color[idx]
        else:
            args = ", ".join(
                [f"vec2({p.position}, {p.color[idx]})" for p in cr.elements])
            return f"lookup({lookup_type}, true, {t}, {args})"

    r_func = gen_c(0)
    g_func = gen_c(1)
    b_func = gen_c(2)
    a_func = gen_c(3)

    return f"color({r_func}, {g_func}, {b_func}, {a_func})"


def _export_rgb_to_bw(ctx, node):
    color = export_node(ctx, node.inputs["Color"])
    return f"luminance({color})"


def _curve_lookup(curve, t, interpolate, extrapolate):
    # No points given, return zero
    if len(curve.points) == 0:
        return 0

    # Just a constant curve
    if len(curve.points) == 1:
        return curve.points[0].location[1]

    # Check if it is just the default one which we can skip
    if len(curve.points) == 2:
        if curve.points[0].location[0] == 0 and curve.points[0].location[1] == 0 and curve.points[1].location[0] == 1 and curve.points[1].location[1] == 1:
            return t

    lookup_type = "'linear'" if interpolate else "'constant'"
    extrapolate = "true" if extrapolate else "false"

    args = ", ".join(
        [f"vec2({p.location[0]}, {p.location[1]})" for p in curve.points])

    return f"lookup({lookup_type}, {extrapolate}, {t}, {args})"


def _export_float_curve(ctx, node):
    mapping = node.mapping

    fac = export_node(ctx, node.inputs[0])
    value = export_node(ctx, node.inputs[1])

    V = mapping.curves[0]

    lV = _curve_lookup(V, value, True, mapping.extend)

    e_f = try_extract_node_value(fac, default=-1)
    if e_f == 1:
        return lV
    elif e_f == 0:
        return value
    else:
        return f"mix({value}, {lV}, {fac})"


def _export_rgb_curve(ctx, node):
    mapping = node.mapping

    fac = export_node(ctx, node.inputs[0])
    color = export_node(ctx, node.inputs[1])

    C = mapping.curves[3]
    R = mapping.curves[0]
    G = mapping.curves[1]
    B = mapping.curves[2]

    lCR = _curve_lookup(C, f"{color}.r", True, mapping.extend)
    lR = _curve_lookup(R, lCR, True, mapping.extend)
    lCG = _curve_lookup(C, f"{color}.g", True, mapping.extend)
    lG = _curve_lookup(G, lCG, True, mapping.extend)
    lCB = _curve_lookup(C, f"{color}.b", True, mapping.extend)
    lB = _curve_lookup(B, lCB, True, mapping.extend)

    ops = f"color({lR}, {lG}, {lB})"

    e_f = try_extract_node_value(fac, default=-1)
    if e_f == 1:
        return ops
    elif e_f == 0:
        return color
    else:
        return f"mix({color}, {ops}, {fac})"


def _export_vector_curve(ctx, node):
    mapping = node.mapping

    fac = export_node(ctx, node.inputs[0])
    vector = export_node(ctx, node.inputs[1])

    X = mapping.curves[0]
    Y = mapping.curves[1]
    Z = mapping.curves[2]

    lX = _curve_lookup(X, f"{vector}.x", True, mapping.extend)
    lY = _curve_lookup(Y, f"{vector}.y", True, mapping.extend)
    lZ = _curve_lookup(Z, f"{vector}.z", True, mapping.extend)

    ops = f"vec3({lX}, {lY}, {lZ})"

    e_f = try_extract_node_value(fac, default=-1)
    if e_f == 1:
        return ops
    elif e_f == 0:
        return vector
    else:
        return f"mix({vector}, {ops}, {fac})"


def _export_vector_mapping(ctx, node):  # TRS
    if node.inputs["Vector"].is_linked:
        vec = export_node(ctx, node.inputs["Vector"])
    else:
        vec = 'vec3(0)'

    sca = export_node(ctx, node.inputs["Scale"])
    rot = export_node(ctx, node.inputs["Rotation"])

    skip_sca = check_socket_if_constant(node.inputs["Scale"], value=1)
    skip_rot = check_socket_if_constant(node.inputs["Rotation"], value=0)
    skip_loc = check_socket_if_constant(node.inputs["Location"], value=0)

    if node.vector_type == 'POINT':
        loc = export_node(ctx, node.inputs["Location"])
        if not skip_sca:
            out = f"({vec} * {sca})"
        else:
            out = vec

        if not skip_rot:
            out = f"rotate_euler({out}, {rot})"

        if not skip_loc:
            return f"({out} + {loc})"
        else:
            return out
    elif node.vector_type == 'TEXTURE':
        loc = export_node(ctx, node.inputs["Location"])
        if not skip_loc:
            out = f"({vec} - {loc})"
        else:
            out = vec

        if not skip_rot:
            out = f"rotate_euler_inverse({out}, {rot})"

        if not skip_sca:
            return f"({out} / {sca})"
        else:
            return out
    elif node.vector_type == 'NORMAL':
        if not skip_sca:
            out = f"({vec} / {sca})"
        else:
            out = vec
        if not skip_rot:
            out = f"rotate_euler({out}, {rot})"
        return f"norm({out})"
    else:
        if not skip_sca:
            out = f"({vec} * {sca})"
        else:
            out = vec
        if not skip_rot:
            return f"rotate_euler({out}, {rot})"
        else:
            return out


def _export_vector_math(ctx, node):
    if node.operation == "ADD":
        return f"({export_node(ctx, node.inputs[0])} + {export_node(ctx, node.inputs[1])})"
    elif node.operation == "SUBTRACT":
        return f"({export_node(ctx, node.inputs[0])} - {export_node(ctx, node.inputs[1])})"
    elif node.operation == "MULTIPLY":
        return f"({export_node(ctx, node.inputs[0])} * {export_node(ctx, node.inputs[1])})"
    elif node.operation == "SCALE":
        return f"({export_node(ctx, node.inputs[0])} * {export_node(ctx, node.inputs[1])})"
    elif node.operation == "DIVIDE":
        return f"({export_node(ctx, node.inputs[0])} / {export_node(ctx, node.inputs[1])})"
    elif node.operation == "MULTIPLY_ADD":
        return f"(({export_node(ctx, node.inputs[0])} * {export_node(ctx, node.inputs[1])}) + {export_node(ctx, node.inputs[2])})"
    elif node.operation == "CROSS_PRODUCT":
        return f"cross({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "PROJECT":
        return f"project({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "REFLECT":
        return f"reflect({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "REFRACT":
        return f"refract({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])}, {export_node(ctx, node.inputs[2])})"
    elif node.operation == "FACEFORWARD":
        A = export_node(ctx, node.inputs[0])
        return f"select(dot({export_node(ctx, node.inputs[1])}, {export_node(ctx, node.inputs[2])}) < 0, {A}, -{A})"
    elif node.operation == "DOT_PRODUCT":
        return f"dot({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "DISTANCE":
        return f"dist({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "LENGTH":
        return f"length({export_node(ctx, node.inputs[0])})"
    elif node.operation == "NORMALIZE":
        return f"norm({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ABSOLUTE":
        return f"abs({export_node(ctx, node.inputs[0])})"
    elif node.operation == "EXPONENT":
        return f"exp({export_node(ctx, node.inputs[0])})"
    elif node.operation == "MINIMUM":
        return f"min({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "MAXIMUM":
        return f"max({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "WRAP":
        return f"wrap({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])}, {export_node(ctx, node.inputs[2])})"
    elif node.operation == "SNAP":
        return f"snap({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "ROUND":
        return f"round({export_node(ctx, node.inputs[0])})"
    elif node.operation == "FLOOR":
        return f"floor({export_node(ctx, node.inputs[0])})"
    elif node.operation == "CEIL":
        return f"ceil({export_node(ctx, node.inputs[0])})"
    elif node.operation == "FRACTION":
        return f"fract({export_node(ctx, node.inputs[0])})"
    elif node.operation == "MODULO":
        return f"fmod({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "SINE":
        return f"sin({export_node(ctx, node.inputs[0])})"
    elif node.operation == "COSINE":
        return f"cos({export_node(ctx, node.inputs[0])})"
    elif node.operation == "TANGENT":
        return f"tan({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCSINE":
        return f"asin({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCCOSINE":
        return f"acos({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCTANGENT":
        return f"atan({export_node(ctx, node.inputs[0])})"
    elif node.operation == "SINH":
        return f"sinh({export_node(ctx, node.inputs[0])})"
    elif node.operation == "COSH":
        return f"cosh({export_node(ctx, node.inputs[0])})"
    elif node.operation == "TANH":
        return f"tanh({export_node(ctx, node.inputs[0])})"
    elif node.operation == "ARCTAN2":
        return f"atan2({export_node(ctx, node.inputs[0])}, {export_node(ctx, node.inputs[1])})"
    elif node.operation == "RADIANS":
        return f"({export_node(ctx, node.inputs[0])} * Pi / 180)"
    elif node.operation == "DEGREES":
        return f"({export_node(ctx, node.inputs[0])} * 180 / Pi)"
    else:
        print(
            f"Not supported vector math operation type {node.operation} for node {node.name}")
        return "vec3(0)"


def _export_vector_rotate(ctx, node):
    vec = export_node(ctx, node.inputs["Vector"])
    center = export_node(ctx, node.inputs["Center"])

    if node.rotation_type == "EULER_XYZ":
        rot = export_node(ctx, node.inputs["Rotation"])
        method = f"rotate_euler_inverse" if node.invert else f"rotate_euler"

        return f"({method}({vec}-{center}, {rot})+{center})"
    else:
        angle = export_node(ctx, node.inputs["Angle"])
        angle = f"-{angle}" if node.invert else angle

        if node.rotation_type == "AXIS_ANGLE":
            axis = export_node(ctx, node.inputs["Axis"])
        elif node.rotation_type == "X_AXIS":
            axis = "vec3(1,0,0)"
        elif node.rotation_type == "Y_AXIS":
            axis = "vec3(0,1,0)"
        elif node.rotation_type == "Z_AXIS":
            axis = "vec3(0,0,1)"

        return f"(rotate_axis({vec}-{center}, {angle}, {axis})+{center})"


def _export_vector_transform(ctx, node):
    map_name = {"WORLD": "global", "OBJECT": "object", "CAMERA": "camera"}

    vec = export_node(ctx, node.inputs["Vector"])
    if node.convert_from == 'CAMERA' or node.convert_to == 'CAMERA':
        print(
            f"Transforming from {node.convert_from} to {node.convert_to} is not supported")
    elif node.convert_from != node.convert_to:
        if node.type == 'Vector':
            func = "transform_direction"
        elif node.type == 'Normal':
            func = "transform_normal"
        else:
            func = "transform_point"
        return f"{func}({vec}, \"{map_name[node.convert_from]}\", \"{map_name[node.convert_to]}\")"

    return vec


def _export_normal(ctx, node, output_name):
    out_norm = _export_default(node.outputs[0])
    if output_name == "Normal":
        return out_norm

    normal = export_node(ctx, node.inputs[0])
    return f"dot({out_norm}, {normal})"


def _export_normalmap(ctx, node):
    # Only supporting tangent space
    if node.space != 'TANGENT' and node.space != 'WORLD':
        print(
            f"Only tangent and world space normal mapping supported")

    color = export_node(ctx, node.inputs["Color"])
    strength = export_node(ctx, node.inputs["Strength"])

    ln = f"(2*{color}-color(1)).xyz"
    if node.space == 'TANGENT':
        dn = f"norm(Nx*({ln}).x + Ny*({ln}).y + N*({ln}).z)"
    else:
        dn = f"norm({ln})"

    if try_extract_node_value(strength) == 1:
        return dn
    elif try_extract_node_value(strength) == 0:
        return "vec3(0)"
    else:
        return f"norm(({dn} - N)*{strength} + N)"


def _export_checkerboard(ctx, node, output_name):
    scale = export_node(ctx, node.inputs["Scale"])

    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
    else:
        uv = TEXCOORD_UV

    raw = f"checkerboard({uv} * {scale})"
    if output_name == "Color":
        color1 = export_node(ctx, node.inputs["Color1"])
        color2 = export_node(ctx, node.inputs["Color2"])
        return f"select({raw} == 1, {color1}, {color2})"
    else:
        return raw


def _export_image(image, path, is_f32=False, keep_format=False):
    # Make sure the image is loaded to memory, so we can write it out
    if not image.has_data:
        image.pixels[0]

    # Export the actual image data
    try:
        old_path = image.filepath_raw
        old_format = image.file_format
        try:
            image.filepath_raw = path
            if not keep_format:
                image.file_format = "PNG" if not is_f32 else "OPEN_EXR"
            image.save()
        finally:  # Never break the scene!
            image.filepath_raw = old_path
            image.file_format = old_format
    except:
        if not keep_format:
            raise RuntimeError(
                "Can not change the original format of given image")
        # Try other way
        image.save_render(path)


def _handle_image(ctx, image):
    if image.source == 'GENERATED':
        img_name = image.name + \
            (".png" if not image.use_generated_float else ".exr")
        img_path = os.path.join("Textures", img_name)
        if img_name not in ctx.result["_images"]:
            _export_image(image, os.path.join(ctx.path, img_path),
                          is_f32=image.use_generated_float)
            ctx.result["_images"].add(img_name)

        return img_path
    elif image.source == 'FILE':
        img_path = bpy.path.relpath(bpy.path.abspath(bpy.path.resolve_ncase(
            image.filepath_raw), library=image.library), start=ctx.path).replace("\\", "/")
        if img_path.startswith("//"):
            img_path = img_path[2:]

        export_image = image.packed_file or ctx.always_copy_images or img_path == ''

        if export_image:
            img_name = bpy.path.basename(img_path)

            # Special case: We can not export PNG if bit depth is not 8 (or 32), for whatever reason

            if img_name == '' or image.depth > 32 or image.depth == 16:
                keep_format = False
                if image.depth > 32 or image.depth == 16 or image.file_format in ["OPEN_EXR", "OPEN_EXR_MULTILAYER", "HDR"]:
                    is_f32 = True
                    extension = ".exr"
                else:
                    is_f32 = False
                    extension = ".png"
                img_path = os.path.join("Textures", image.name + extension)
            else:
                keep_format = True
                is_f32 = False  # Does not matter
                img_path = os.path.join("Textures", img_name)

            if img_name not in ctx.result["_images"]:
                try:
                    _export_image(image, os.path.join(ctx.path, img_path),
                                  is_f32=is_f32, keep_format=keep_format)
                except:
                    # Above failed, so give this a try
                    img_path = os.path.join("Textures", img_name)
                    _export_image(image, os.path.join(ctx.path, img_path),
                                  is_f32=False, keep_format=False)
                ctx.result["_images"].add(img_name)
        return img_path
    else:
        print(f"Image type {image.source} not supported")
        return None


def _export_image_texture(ctx, node):
    id = len(ctx.result["textures"])
    tex_name = f"_tex_{id}"

    if not node.image:
        print(f"Image node {node.name} has no image")
        return "color(0)"

    img_path = _handle_image(ctx, node.image)

    if node.extension == "EXTEND":
        wrap_mode = "clamp"
    elif node.extension == "CLIP":
        wrap_mode = "clamp"  # Not really supported
    else:
        wrap_mode = "repeat"

    if node.interpolation == "Closest":
        filter_type = "nearest"
    else:
        filter_type = "bilinear"

    cs = node.image.colorspace_settings.name
    if cs == "Linear" or cs == "Non-Color" or cs == "Raw":
        linear = True
    elif cs == "sRGB":
        linear = False
    else:
        print(f"Not supported image color space {cs}")
        linear = False

    # Make sure we do not add the same texture to the array.
    # This is not necessary for memory or image loading,
    # but can improve performance due to less shader complexity
    key = (img_path, wrap_mode, filter_type, linear)
    if key in ctx.result["_image_textures"]:
        tex_name = ctx.result["_image_textures"][key]
    else:
        ctx.result["textures"].append(
            {
                "type": "image",
                "name": tex_name,
                "filename": img_path,
                "wrap_mode": wrap_mode,
                "filter_type": filter_type,
                "linear": linear
            }
        )
        ctx.result["_image_textures"][key] = tex_name

    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
        tex_access = f"{tex_name}(({uv}).xy)"
    else:
        tex_access = tex_name

    return tex_access


def _get_noise_vector(ctx, node):
    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
    else:
        uv = TEXCOORD_UV

    if node.noise_dimensions == '1D':
        w = export_node(ctx, node.inputs["W"])
        return f"{w}"
    elif node.noise_dimensions == '2D':
        return f"{uv}.xy"
    elif node.noise_dimensions == '3D':
        return f"{uv}"
    else:
        print(f"Four dimensional noise is not supported")
        return f"{uv}"


def _export_white_noise(ctx, node, output_name):
    ops = _get_noise_vector(ctx, node)

    if output_name == "Color":
        return f"cnoise({ops})"
    else:
        return f"noise({ops})"


def _export_noise(ctx, node, output_name):
    # TODO: Missing Detail, Roughness and Distortion
    ops = _get_noise_vector(ctx, node)
    scale = export_node(ctx, node.inputs["Scale"])

    if output_name == "Color":
        return f"cpnoise(abs({ops}*{scale}))"
    else:
        return f"pnoise(abs({ops}*{scale}))"


def _export_voronoi(ctx, node, output_name):
    # TODO: Missing a lot of parameters
    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
    else:
        uv = TEXCOORD_UV

    ops = f"{uv}.xy"
    if node.voronoi_dimensions != '2D':
        print(f"Voronoi currently only supports 2d vectors")
        return f"{uv}"

    scale = export_node(ctx, node.inputs["Scale"])

    if output_name == "Color":
        return f"cvoronoi(abs({ops}*{scale}))"
    elif output_name == "Position":
        # TODO
        return f"vec3(voronoi(abs({ops}*{scale})))"
    else:
        return f"voronoi(abs({ops}*{scale}))"


def _export_musgrave(ctx, node, output_name):
    # TODO: Missing a lot of parameters and only supports fbm
    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
    else:
        uv = TEXCOORD_UV

    ops = f"{uv}.xy"
    if node.musgrave_dimensions != '2D':
        print(f"Musgrave currently only supports 2d vectors")
        return f"{uv}"

    scale = export_node(ctx, node.inputs["Scale"])

    return f"fbm(abs({ops}*{scale}))"


def _export_wave(ctx, node, output_name):
    # TODO: Add distortion
    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
    else:
        uv = TEXCOORD_UV

    scale = export_node(ctx, node.inputs["Scale"])
    uv = f"({uv}*{scale})"

    if node.wave_type == "BANDS":
        if node.bands_direction == "X":
            coord = f"{uv}.x*20"
        elif node.bands_direction == "Y":
            coord = f"{uv}.y*20"
        elif node.bands_direction == "Z":
            coord = f"{uv}.z*20"
        else:  # DIAGONAL
            coord = f"sum({uv})*10"
    else:  # RINGS
        if node.bands_direction == "X":
            coord = f"length({uv}.yz)*20"
        elif node.bands_direction == "Y":
            coord = f"length({uv}.xz)*20"
        elif node.bands_direction == "Z":
            coord = f"length({uv}.xy)*20"
        else:  # DIAGONAL
            coord = f"length({uv})*20"

    phase = export_node(ctx, node.inputs["Phase Offset"])
    coord = f"({coord} + {phase})"

    if node.wave_profile == "SIN":
        ops = f"(0.5 + 0.5 * sin({coord} - Pi/2))"
    elif node.wave_profile == "SAW":
        ops = f"fract(0.5*{coord}/Pi)"
    else:  # TRI
        ops = f"2*abs(0.5*{coord}/Pi - floor(0.5*{coord}/Pi + 0.5))"

    if output_name == "Color":
        return f"color({ops})"
    else:
        return ops


def _export_environment(ctx, node):
    # TODO: Currently no support for projection mode
    id = len(ctx.result["textures"])
    tex_name = f"_tex_{id}"

    if not node.image:
        print(f"Image node {node.name} has no image")
        return "color(0)"

    img_path = _handle_image(ctx, node.image)

    wrap_mode = "clamp"

    if node.interpolation == "Closest":
        filter_type = "nearest"
    else:
        filter_type = "bilinear"

    ctx.result["textures"].append(
        {
            "type": "image",
            "name": tex_name,
            "filename": img_path,
            "wrap_mode": wrap_mode,
            "filter_type": filter_type
        }
    )

    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
        tex_access = f"{tex_name}(({uv}).xy)"
    else:
        tex_access = tex_name

    return tex_access


def _export_gradient(ctx, node, output_name):
    if node.inputs["Vector"].is_linked:
        uv = export_node(ctx, node.inputs["Vector"])
    else:
        uv = TEXCOORD_UV

    if node.gradient_type == "LINEAR":
        fac = f"{uv}.x"
    elif node.gradient_type == "QUADRATIC":
        fac = f"(max(0, {uv}.x)^2)"
    elif node.gradient_type == "EASING":
        t = f"clamp({uv}.x,0,1)"
        fac = f"(3*{t}^2 - 2*{t}^3)"
    elif node.gradient_type == "DIAGONAL":
        fac = f"avg({uv}.xy)"
    elif node.gradient_type == "SPHERICAL":
        fac = f"max(0, 1-length({uv}))"
    elif node.gradient_type == "QUADRATIC_SPHERE":
        fac = f"(max(0, 1-length({uv}))^2)"
    else:  # RADIAL
        fac = f"(0.5*atan2({uv}.y, {uv}.x) / Pi + 0.5)"

    fac = f"clamp({fac}, 0, 1)"
    if output_name == "Color":
        return f"color({fac})"
    else:
        return fac


def _export_surface_attributes(ctx, node, output_name):
    if output_name == "Position":
        return "P"
    elif output_name == "Normal":
        return "N"
    elif output_name == "Tangent":
        return "Nx"
    elif output_name == "True Normal":
        return "Ng"
    elif output_name == "Incoming":
        return "V"
    elif output_name == "Parametric":
        return "vec3(prim_coords.x, prim_coords.y, 0)"
    elif output_name == "Backfacing":
        return "select(frontside, 0, 1)"
    else:
        print(f"Given geometry attribute '{output_name}' not supported")
        return "N"


def _export_tex_coordinate(ctx, node, output_name):
    if output_name == 'UV':
        return TEXCOORD_UV
    elif output_name == 'Generated':
        return "Np"
    elif output_name == 'Normal':
        return 'transform_normal(N, "global", "object")'
    elif output_name == 'Object':
        # TODO: Handle other objects?
        return 'transform_point(P, "global", "object")'
    elif output_name == 'Reflection':
        return 'reflect(V, N)'
    else:
        print(
            f"Given texture coordinate output '{output_name}' is not supported")
        return TEXCOORD_UV


def _export_uvmap(ctx, node):
    # TODO: Maybe support this with texture lookups?
    if node.uv_map != "":
        print(f"Additional UV Maps are not supported, default to first one")
    return TEXCOORD_UV


def _export_object_info(ctx, node, output_name):
    if output_name != 'Random':
        print(f"Object info node only supports 'Random' attribute")
    return "noise(entity_id)"


def _export_light_path(ctx, node, output_name):
    print(f"Light path output not fully supported, returning default values if necessary")
    # No light paths supported, so just return some default values
    # This will work 90% of the time, but most likely will fail in some cases
    if output_name == "Is Camera Ray":
        return "select(check_ray_flag(\"camera\"), 1, 0)"
    elif output_name == "Is Shadow Ray":
        return "select(check_ray_flag(\"shadow\"), 1, 0)"
    elif output_name == "Is Diffuse Ray":
        return 1
    elif output_name == "Is Glossy Ray":
        return 1
    elif output_name == "Is Singular Ray":
        return 0  # Dielectrics & Conductors with roughness=0 are singular, but we ignore them
    elif output_name == "Is Reflection Ray":
        return "select(check_ray_flag(\"bounce\"), 1, 0)"
    elif output_name == "Is Transmission Ray":
        return "select(check_ray_flag(\"bounce\"), 1, 0)"
    elif output_name == "Ray Length":
        return 1
    elif output_name == "Ray Depth":
        return 1
    elif output_name == "Diffuse Depth":
        return 1
    elif output_name == "Glossy Depth":
        return 1
    elif output_name == "Transparent Depth":
        return 0
    elif output_name == "Transmission Depth":
        return 0
    else:
        print(f"Unknown light path output {output_name}")
        return 0


def handle_node_group_begin(ctx, node, output_name, func):
    if node.node_tree is None:
        print(
            f"Invalid group node '{node.name}'")
        return None

    output = node.node_tree.nodes.get("Group Output")
    if output is None:
        print(
            f"Invalid group '{node.node_tree.name}'")
        return None

    # print(f"Begin group {node.node_tree.name} via {output_name}")
    ctx.push(node)
    out = func(ctx, output.inputs[output_name])
    ctx.pop()
    return out


def handle_node_group_end(ctx, node, output_name, func):
    grp = ctx.current
    # print(f"End group {grp.node_tree.name} via {output_name}")
    ctx.goIn()
    out = func(ctx, grp.inputs[output_name])
    ctx.goOut()
    return out


def handle_node_reroute(ctx, node, func):
    return func(ctx, node.inputs[0])


def handle_node_implicit_mappings(socket, expr):
    to_type = socket.type
    from_type = socket.links[0].from_socket.type
    if to_type == from_type:
        return expr
    elif (from_type == 'VALUE' or from_type == 'INT') and (to_type == 'RGBA' or to_type == 'SHADER'):
        return f"color({expr})"
    elif from_type == 'RGBA' and (to_type == 'VALUE' or to_type == 'INT'):
        return f"luminance({expr})"
    elif (from_type == 'VALUE' or from_type == 'INT') and to_type == 'VECTOR':
        return f"vec3({expr})"
    elif from_type == 'VECTOR' and (to_type == 'VALUE' or to_type == 'INT'):
        return f"avg({expr})"
    elif from_type == 'RGBA' and to_type == 'VECTOR':
        return f"({expr}).rgb"
    elif from_type == 'VECTOR' and (to_type == 'RGBA' or to_type == 'SHADER'):
        return f"max(color({expr}.x, {expr}.y, {expr}.z, 1), color(0))"
    else:
        print(
            f"Socket connection from {socket.links[0].from_socket.name} to {socket.name} requires cast from {from_type} to {to_type} which is not supported")
        return expr


def export_node(ctx, socket):
    # Missing:
    # ShaderNodeAttribute, ShaderNodeBevel, ShaderNodeBump, ShaderNodeCameraData,
    # ShaderNodeCustomGroup, ShaderNodeHairInfo, ShaderNodeLightFalloff,
    # ShaderNodeObjectInfo, ShaderNodeScript,
    # ShaderNodeShaderToRGB, ShaderNodeSubsurfaceScattering, ShaderNodeTangent,
    # ShaderNodeTexBrick, ShaderNodeTexIES, ShaderNodeTexMagic,
    # ShaderNodeTexPointDensity, ShaderNodeTexSky, ShaderNodeUVAlongStroke,
    # ShaderNodeVectorDisplacement, ShaderNodeVertexColor, ShaderNodeWavelength, ShaderNodeWireframe

    # No support planned:
    # ShaderNodeAmbientOcclusion, ShaderNodeLightPath, ShaderNodeOutputAOV,
    # ShaderNodeParticleInfo, ShaderNodeOutputLineStyle, ShaderNodePointInfo, ShaderNodeHoldout

    if socket.is_linked:
        if socket in ctx.cache:
            return ctx.cache[socket]

        node = socket.links[0].from_node
        output_name = socket.links[0].from_socket.name

        if isinstance(node, bpy.types.ShaderNodeTexImage):
            expr = _export_image_texture(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeTexChecker):
            expr = _export_checkerboard(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeTexCoord):
            expr = _export_tex_coordinate(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeTexNoise):
            expr = _export_noise(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeTexWhiteNoise):
            expr = _export_white_noise(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeTexVoronoi):
            expr = _export_voronoi(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeTexMusgrave):
            expr = _export_musgrave(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeTexWave):
            expr = _export_wave(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeTexEnvironment):
            expr = _export_environment(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeTexGradient):
            expr = _export_gradient(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeNewGeometry):
            expr = _export_surface_attributes(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeFresnel):
            expr = _export_fresnel(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeLayerWeight):
            expr = _export_layer_weight(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeMath):
            expr = _export_scalar_math(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeValue):
            expr = _export_scalar_value(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeClamp):
            expr = _export_scalar_clamp(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeMapRange):
            expr = _export_maprange(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeMixRGB):
            expr = _export_rgb_math(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeInvert):
            expr = _export_rgb_invert(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeGamma):
            expr = _export_rgb_gamma(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeBrightContrast):
            expr = _export_rgb_brightcontrast(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeHueSaturation):
            expr = _export_hsv(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeBlackbody):
            expr = _export_blackbody(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeRGB):
            expr = _export_rgb_value(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeValToRGB):
            expr = _export_val_to_rgb(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeRGBToBW):
            expr = _export_rgb_to_bw(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeCombineHSV):
            expr = _export_combine_hsv(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeCombineRGB):
            expr = _export_combine_rgb(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeCombineXYZ):
            expr = _export_combine_xyz(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeSeparateHSV):
            expr = _export_separate_hsv(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeSeparateRGB):
            expr = _export_separate_rgb(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeSeparateXYZ):
            expr = _export_separate_xyz(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeFloatCurve):
            expr = _export_float_curve(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeRGBCurve):
            expr = _export_rgb_curve(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeVectorCurve):
            expr = _export_vector_curve(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeMapping):
            expr = _export_vector_mapping(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeVectorMath):
            expr = _export_vector_math(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeVectorRotate):
            expr = _export_vector_rotate(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeVectorTransform):
            expr = _export_vector_transform(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeNormal):
            expr = _export_normal(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeNormalMap):
            expr = _export_normalmap(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeUVMap):
            expr = _export_uvmap(ctx, node)
        elif isinstance(node, bpy.types.ShaderNodeObjectInfo):
            expr = _export_object_info(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeLightPath):
            expr = _export_light_path(ctx, node, output_name)
        elif isinstance(node, bpy.types.ShaderNodeGroup):
            expr = handle_node_group_begin(ctx, node, output_name, export_node)
        elif isinstance(node, bpy.types.NodeGroupInput):
            expr = handle_node_group_end(ctx, node, output_name, export_node)
        elif isinstance(node, bpy.types.NodeReroute):
            expr = handle_node_reroute(ctx, node, export_node)
        else:
            print(
                f"Shader node {node.name} of type {type(node).__name__} is not supported")
            expr = None

        if expr is None:
            return _export_default(socket)

        full_expr = handle_node_implicit_mappings(socket, expr)

        ctx.cache[socket] = full_expr
        return full_expr
    else:
        return _export_default(socket)
