from .pyignis import *

_g_width = 0
_g_height = 0


class Tracer:
    def __init__(self, rays):
        self.count = len(rays)
        self.rays = rays

        global _g_width
        global _g_height
        if _g_width > 0 or _g_height > 0:
            if _g_width != self.count or _g_height != 1:
                raise RuntimeError("No multiple instances of Ignis allowed!")
            # Silently accept -> useful for notebooks
        else:
            setup(self.count, 1)
            _g_width = self.count
            _g_height = 1

    def __del__(self):
        global _g_width
        global _g_height
        self._cleanup()
        _g_width = 0
        _g_height = 0

    def _cleanup(self):
        cleanup()

    def trace(self, n=1):
        clear_pixels()
        res = [[0 for x in range(3)] for x in range(self.count)]
        for i in range(0, n):
            output = trace(self.rays)
            for j in range(0, self.count):
                res[j][0] += output[3*j + 0] / n
                res[j][1] += output[3*j + 1] / n
                res[j][2] += output[3*j + 2] / n
        return res

    def add_resource_path(self, path):
        add_resource_path(path)
