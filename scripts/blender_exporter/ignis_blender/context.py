import bpy


class ExportContext:
    ''' Context containing all information regarding the current export process '''

    def __init__(self, rootpath: str, op: bpy.types.Operator | bpy.types.RenderEngine, depsgraph: bpy.types.Depsgraph, settings):
        self.root = rootpath
        self.operator = op
        self.depsgraph = depsgraph
        self.settings = settings

    def report(self, type, text: str):
        if self.operator is not None:
            self.operator.report(type, text)
        else:
            print(f"[{type[0]}] Ignis: {text}")

    def report_warning(self, text: str):
        self.report({'WARNING'}, text)

    def report_error(self, text: str):
        self.report({'ERROR'}, text)

    def report_info(self, text: str):
        self.report({'INFO'}, text)

    @property
    def scene(self):
        return self.depsgraph.scene

    @property
    def is_renderer(self):
        return getattr(self.scene, 'ignis', None) is not None and bpy.context.engine == 'IGNIS_RENDER'


class NodeContext:
    ''' Context containing all information regarding a single node expansion, requires a parent export context '''

    def __init__(self, result: {}, export_ctx: ExportContext):
        self.result = result
        self.exporter = export_ctx
        self._cache = {}
        self.offset_texcoord = (0, 0, 0)

        self._stack = []
        self._cur = 0

    def push(self, node: bpy.types.Node):
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

    def shift_texcoord(self, dx=0, dy=0, dz=0):
        self.offset_texcoord = (
            self.offset_texcoord[0]+dx, self.offset_texcoord[1]+dy, self.offset_texcoord[2]+dz)

    @property
    def texcoord(self):
        if self.offset_texcoord[0] == 0 and self.offset_texcoord[1] == 0 and self.offset_texcoord[2] == 0:
            return "uvw"
        else:
            return f"(vec3({self.offset_texcoord[0]}, {self.offset_texcoord[1]}, {self.offset_texcoord[2]}) + uvw)"

    def get_from_cache(self, socket: bpy.types.NodeSocket):
        return self._cache.get((socket, self.offset_texcoord))

    def put_in_cache(self, socket: bpy.types.NodeSocket, expr: str):
        self._cache[(socket, self.offset_texcoord)] = expr

    @property
    def scene(self):
        return self.exporter.scene

    @property
    def path(self):
        return self.exporter.root

    @property
    def settings(self):
        return self.exporter.settings

    def report_warning(self, text: str):
        self.exporter.report_warning(text)

    def report_error(self, text: str):
        self.exporter.report_error(text)

    def report_info(self, text: str):
        self.exporter.report_info(text)
