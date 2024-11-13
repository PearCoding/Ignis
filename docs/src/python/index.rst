Python API
==========

.. _Ignis (module):

Ignis (module)
-----------------------------------------------

Ignis python interface


.. _BoundingBox:

BoundingBox
-----------------------------------------------

BoundingBox

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Center: (self) -> Vec3`

- :pythonfunc:`Diameter: (self) -> Vec3`

- :pythonfunc:`HalfArea: (self) -> float`

- :pythonfunc:`IsEmpty: (self) -> bool`

- :pythonfunc:`Max: (self) -> Vec3`

- :pythonfunc:`Min: (self) -> Vec3`

- :pythonfunc:`Volume: (self) -> float`

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`extend(self, arg: {BoundingBox}) -> {BoundingBox}`

- :pythonfunc:`extend(self, arg: Vec3) -> {BoundingBox}`

- :pythonfunc:`inflate(self, arg: float) -> None`

- :pythonfunc:`isInside(self, arg: Vec3) -> bool`

- :pythonfunc:`isOverlapping(self, arg: {BoundingBox}) -> bool`

- :pythonfunc:`overlap(self, arg: {BoundingBox}) -> {BoundingBox}`


.. _CPUArchitecture:

CPUArchitecture
-----------------------------------------------

Enum holding supported CPU architectures


.. _CameraOrientation:

CameraOrientation
-----------------------------------------------

General camera orientation

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Dir: Direction the camera is facing`

- :pythonfunc:`Eye: Origin of the camera`

- :pythonfunc:`Up: Vector defining the up of the camera`


.. _DenoiserSettings:

DenoiserSettings
-----------------------------------------------

Settings for the denoiser

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Enabled: Enable or disable the denoiser`

- :pythonfunc:`HighQuality: Set True if denoiser should be high quality or interactive`

- :pythonfunc:`Prefilter: Set True if normal and albedo layer should be prefiltered`


.. _GPUArchitecture:

GPUArchitecture
-----------------------------------------------

Enum holding supported GPU architectures


.. _LogLevel:

LogLevel
-----------------------------------------------

Enum holding verbosity level for logging


.. _Ray:

Ray
-----------------------------------------------

Single ray traced into the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Direction: Direction of the ray`

- :pythonfunc:`Origin: Origin of the ray`

- :pythonfunc:`Range: Range (tmin, tmax) of the ray`


.. _Runtime:

Runtime
-----------------------------------------------

Renderer runtime allowing control of simulation and access to results

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`AOVs: (self) -> list[str]`

- :pythonfunc:`Camera: (self) -> str`

- :pythonfunc:`ColorParameters: (self) -> dict[str, Vec4]`

- :pythonfunc:`FloatParameters: (self) -> dict[str, float]`

- :pythonfunc:`FrameCount: (self) -> int`

- :pythonfunc:`FramebufferHeight: (self) -> int`

- :pythonfunc:`FramebufferWidth: (self) -> int`

- :pythonfunc:`InitialCameraOrientation: (self) -> {CameraOrientation}`

- :pythonfunc:`IntParameters: (self) -> dict[str, int]`

- :pythonfunc:`IterationCount: (self) -> int`

- :pythonfunc:`RenderStartTime: (self) -> int`

- :pythonfunc:`SPI: (self) -> int`

- :pythonfunc:`SampleCount: (self) -> int`

- :pythonfunc:`SceneBoundingBox: (self) -> {BoundingBox}`

- :pythonfunc:`Seed: (self) -> int`

- :pythonfunc:`StringParameters: (self) -> dict[str, str]`

- :pythonfunc:`Target: (self) -> {Target}`

- :pythonfunc:`Technique: (self) -> str`

- :pythonfunc:`VectorParameters: (self) -> dict[str, Vec3]`

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`clearFramebuffer(self) -> None`

- :pythonfunc:`clearFramebuffer(self, arg: str) -> None`

- :pythonfunc:`getFramebufferForDevice(self, aov: str = '') -> Image`

- :pythonfunc:`getFramebufferForHost(self, aov: str = '') -> CPUImage`

- :pythonfunc:`incFrameCount(self) -> None`

- :pythonfunc:`reset(self) -> None`

- :pythonfunc:`saveFramebuffer(self, arg: str | os.PathLike) -> bool`

- :pythonfunc:`setCameraOrientation(self, arg: {CameraOrientation}) -> None`

- :pythonfunc:`setParameter(self, arg0: str, arg1: int) -> None`

- :pythonfunc:`setParameter(self, arg0: str, arg1: float) -> None`

- :pythonfunc:`setParameter(self, arg0: str, arg1: Vec3) -> None`

- :pythonfunc:`setParameter(self, arg0: str, arg1: Vec4) -> None`

- :pythonfunc:`setParameter(self, arg0: str, arg1: str) -> None`

- :pythonfunc:`step(self, ignoreDenoiser: bool = False) -> None`

- :pythonfunc:`tonemap(self, arg: CPUArray2d_UInt32) -> None`

- :pythonfunc:`trace(self, arg: list[{Ray}]) -> list[Vec3]`


.. _RuntimeOptions:

RuntimeOptions
-----------------------------------------------

Options to customize runtime behaviour

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`AcquireStats: Set True if statistical data should be acquired while rendering`

- :pythonfunc:`Denoiser: Settings for the denoiser`

- :pythonfunc:`DumpShader: Set True if most shader should be dumped into the filesystem`

- :pythonfunc:`DumpShaderFull: Set True if all shader should be dumped into the filesystem`

- :pythonfunc:`EnableTonemapping: Set True if any of the two tonemapping functions ``tonemap`` and ``imageinfo`` is to be used`

- :pythonfunc:`OverrideCamera: Type of camera to use instead of the one used by the scene`

- :pythonfunc:`OverrideFilmSize: Type of film size to use instead of the one used by the scene`

- :pythonfunc:`OverrideTechnique: Type of technique to use instead of the one used by the scene`

- :pythonfunc:`SPI: The requested sample per iteration. Can be 0 to set automatically`

- :pythonfunc:`Seed: Seed for the random generators`

- :pythonfunc:`Target: The target device`

- :pythonfunc:`WarnUnused: Set False if you want to ignore warnings about unused property entries`


.. _RuntimeWrap:

RuntimeWrap
-----------------------------------------------

Wrapper around the runtime used for proper runtime loading and shutdown

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`instance: (self) -> {Runtime}`

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`shutdown(self) -> None`


.. _Scene:

Scene
-----------------------------------------------

Class representing a whole scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`bsdfs: (self) -> dict[str, {SceneObject}]`

- :pythonfunc:`camera: (self) -> {SceneObject}`

- :pythonfunc:`entities: (self) -> dict[str, {SceneObject}]`

- :pythonfunc:`film: (self) -> {SceneObject}`

- :pythonfunc:`lights: (self) -> dict[str, {SceneObject}]`

- :pythonfunc:`media: (self) -> dict[str, {SceneObject}]`

- :pythonfunc:`shapes: (self) -> dict[str, {SceneObject}]`

- :pythonfunc:`technique: (self) -> {SceneObject}`

- :pythonfunc:`textures: (self) -> dict[str, {SceneObject}]`

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`addBSDF(self, arg0: str, arg1: {SceneObject}) -> None`

- :pythonfunc:`addConstantEnvLight(self) -> None`

- :pythonfunc:`addEntity(self, arg0: str, arg1: {SceneObject}) -> None`

- :pythonfunc:`addFrom(self, arg: {Scene}) -> None`

- :pythonfunc:`addLight(self, arg0: str, arg1: {SceneObject}) -> None`

- :pythonfunc:`addMedium(self, arg0: str, arg1: {SceneObject}) -> None`

- :pythonfunc:`addShape(self, arg0: str, arg1: {SceneObject}) -> None`

- :pythonfunc:`addTexture(self, arg0: str, arg1: {SceneObject}) -> None`

- :pythonfunc:`bsdf(self, arg: str) -> {SceneObject}`

- :pythonfunc:`entity(self, arg: str) -> {SceneObject}`

- :pythonfunc:`light(self, arg: str) -> {SceneObject}`

- :pythonfunc:`medium(self, arg: str) -> {SceneObject}`

- :pythonfunc:`shape(self, arg: str) -> {SceneObject}`

- :pythonfunc:`texture(self, arg: str) -> {SceneObject}`


.. _SceneObject:

SceneObject
-----------------------------------------------

Class representing an object in the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`baseDir: (self) -> pathlib.Path`

- :pythonfunc:`pluginType: (self) -> str`

- :pythonfunc:`properties: (self) -> dict[str, {SceneProperty}]`

- :pythonfunc:`type: (self) -> {SceneObject.Type}`

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`hasProperty(self, arg: str) -> bool`

- :pythonfunc:`property(self, arg: str) -> {SceneProperty}`

- :pythonfunc:`setProperty(self, arg0: str, arg1: {SceneProperty}) -> None`


.. _SceneParser:

SceneParser
-----------------------------------------------

Parser for standard JSON and glTF scene description

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`loadFromFile(self, path: str | os.PathLike, flags: int = 13303) -> {Scene}`

- :pythonfunc:`loadFromString(self, str: str, opt_dir: str | os.PathLike = '', flags: int = 13303) -> {Scene}`


.. _SceneProperty:

SceneProperty
-----------------------------------------------

Property of an object in the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`type: (self) -> {SceneProperty.Type}`

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`canBeNumber(self) -> bool`

- :pythonfunc:`getBool(self, def: bool = False) -> bool`

- :pythonfunc:`getInteger(self, def: int = 0) -> int`

- :pythonfunc:`getIntegerArray(self) -> list[int]`

- :pythonfunc:`getNumber(self, def: float = 0.0) -> float`

- :pythonfunc:`getNumberArray(self) -> list[float]`

- :pythonfunc:`getString(self, def: str = '') -> str`

- :pythonfunc:`getTransform(self, def: Mat4x4 = Mat4x4.Identity -> Mat4x4`

- :pythonfunc:`getVector2(self, def: Vec2 = Vec2(0)) -> Vec2`

- :pythonfunc:`getVector3(self, def: Vec3 = Vec3(0)) -> Vec3`

- :pythonfunc:`isValid(self) -> bool`


.. _Target:

Target
-----------------------------------------------

Target specification the runtime is using

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Architecture: (self) -> {CPUArchitecture} | {GPUArchitecture}`

- :pythonfunc:`CPUArchitecture: (self) -> {CPUArchitecture}`

- :pythonfunc:`Device: (self) -> int`

- :pythonfunc:`GPUArchitecture: (self) -> {GPUArchitecture}`

- :pythonfunc:`IsCPU: (self) -> bool`

- :pythonfunc:`IsGPU: (self) -> bool`

- :pythonfunc:`IsValid: (self) -> bool`

- :pythonfunc:`ThreadCount: (self) -> int`

- :pythonfunc:`VectorWidth: (self) -> int`

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`toString(self) -> str`


.. _SceneObject-Type:

SceneObject.Type
-----------------------------------------------

Enum holding type of scene object


.. _SceneParser-Flags:

SceneParser.Flags
-----------------------------------------------

Flags modifying parsing behaviour and allowing partial scene loads

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Return integer ratio.`

- :pythonfunc:``

- :pythonfunc:``

- :pythonfunc:`Return a pair of integers, whose ratio is exactly equal to the original int and with a positive denominator.`

- :pythonfunc:``

- :pythonfunc:`>>> (10).as_integer_ratio() (10, 1) >>> (-10).as_integer_ratio() (-10, 1) >>> (0).as_integer_ratio() (0, 1)`

- :pythonfunc:`Number of ones in the binary representation of the absolute value of self.`

- :pythonfunc:``

- :pythonfunc:`Also known as the population count.`

- :pythonfunc:``

- :pythonfunc:`>>> bin(13) '0b1101' >>> (13).bit_count() 3`

- :pythonfunc:`Number of bits necessary to represent self in binary.`

- :pythonfunc:``

- :pythonfunc:`>>> bin(37) '0b100101' >>> (37).bit_length() 6`

- :pythonfunc:`Returns self, the complex conjugate of any int.`

- :pythonfunc:`Return the integer represented by the given array of bytes.`

- :pythonfunc:``

- :pythonfunc:`bytes   Holds the array of bytes to convert.  The argument must either   support the buffer protocol or be an iterable object producing bytes.   Bytes and bytearray are examples of built-in objects that support the   buffer protocol. byteorder   The byte order used to represent the integer.  If byteorder is 'big',   the most significant byte is at the beginning of the byte array.  If   byteorder is 'little', the most significant byte is at the end of the   byte array.  To request the native byte order of the host system, use   `sys.byteorder' as the byte order value.  Default is to use 'big'. signed   Indicates whether two's complement is used to represent the integer.`

- :pythonfunc:`Return an array of bytes representing an integer.`

- :pythonfunc:``

- :pythonfunc:`length   Length of bytes object to use.  An OverflowError is raised if the   integer is not representable with the given number of bytes.  Default   is length 1. byteorder   The byte order used to represent the integer.  If byteorder is 'big',   the most significant byte is at the beginning of the byte array.  If   byteorder is 'little', the most significant byte is at the end of the   byte array.  To request the native byte order of the host system, use   `sys.byteorder' as the byte order value.  Default is to use 'big'. signed   Determines whether two's complement is used to represent the integer.   If signed is False and a negative integer is given, an OverflowError   is raised.`


.. _SceneProperty-Type:

SceneProperty.Type
-----------------------------------------------

Enum holding type of scene property

