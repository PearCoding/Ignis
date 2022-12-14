Realtime Rendering
==================

Ignis is not a rasterization based renderer like the ones used in most modern games. Therefore, it only uses the term *frame* very loosely. A frame consists of the user set ``spp`` (Samples Per Pixel) which is grouped by an iteration given the ``spi`` (Samples per Iteration). Depending on the actual ``spi`` (the user parameter is only considered an hint) the ``spp`` might be increased to make it a multiple of the ``spi``.

Using ``igview`` without an explicitly set ``spp`` (via ``--spp``) makes the rendering progressive, therefore only a single frame will be rendered.

The realtime mode in ``igview`` emulates the frame rendering and can be modified to render in different ways.

Using the ``--spp-mode`` argument allows to set behavior when the target ``spp`` is reached. Available options are:

- ``fixed`` The rendering will finish when the target ``spp`` is reached. Ultimately only rendering a single frame. This is the default.
- ``capped`` The rendering will stop when the target ``spp`` is reached. The application will **not** close. Movement or other state changes will trigger a new rendering, however, ultimately only a single frame will be rendered.
- ``continuos`` The rendering will restart when the target ``spp`` is reached. The frame counter will be increased each restart. Movement or change of the rendering state will reset the frame counter and restart the rendering preemptively.

Using the ``--realtime`` command line argument will set ``spi=1``, ``spp=1`` and the ``spp-mode=continuos``. 
