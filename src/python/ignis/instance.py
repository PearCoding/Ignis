from .pyignis import *

_g_width = 0
_g_height = 0


class Ignis:
    def __init__(self, width=0, height=0):
        if width <= 0 or height <= 0:
            def_settings = default_settings()
            width = def_settings.width
            height = def_settings.height

        global _g_width
        global _g_height
        if _g_width > 0 or _g_height > 0:
            if _g_width != width or _g_height != height:
                raise RuntimeError("No multiple instances of Ignis allowed!")
            # Silently accept -> useful for notebooks
        else:
            setup(width, height)
            _g_width = width
            _g_height = height

        self.iteration = 0

    def __del__(self):
        global _g_width
        global _g_height
        self._cleanup()
        _g_width = 0
        _g_height = 0

    def _cleanup(self):
        cleanup()

    @property
    def pixels(self):
        return pixels()

    def clear(self):
        clear_pixels()
        self.iteration = 0

    def render(self, settings=None):
        self.iteration += 1
        render(self.default_settings()
               if settings is None else settings, self.iteration)

    @property
    def default_settings(self):
        return default_settings().generate()

    def add_resource_path(self, path):
        add_resource_path(path)
