Python API
==========

.. _Ignis (module):

Ignis (module)
-----------------------------------------------

Ignis python interface


.. _CPUArchitecture:

CPUArchitecture
-----------------------------------------------

- :pythonfunc:`Missing Documentation`



.. _CameraOrientation:

CameraOrientation
-----------------------------------------------

General camera orientation

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Dir`: Direction the camera is facing

- :pythonfunc:`Eye`: Origin of the camera

- :pythonfunc:`Up`: Vector defining the up of the camera


.. _GPUArchitecture:

GPUArchitecture
-----------------------------------------------

- :pythonfunc:`Missing Documentation`



.. _Ray:

Ray
-----------------------------------------------

Single ray traced into the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Direction`: Direction of the ray

- :pythonfunc:`Origin`: Origin of the ray

- :pythonfunc:`Range`: Range (tmin, tmax) of the ray


.. _Runtime:

Runtime
-----------------------------------------------

Renderer runtime allowing control of simulation and access to results

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Camera`: <anonymous>(self) -> str

- :pythonfunc:`FrameCount`: <anonymous>(self) -> int

- :pythonfunc:`FramebufferHeight`: <anonymous>(self) -> int

- :pythonfunc:`FramebufferWidth`: <anonymous>(self) -> int

- :pythonfunc:`InitialCameraOrientation`: <anonymous>(self) -> CameraOrientation

- :pythonfunc:`IterationCount`: <anonymous>(self) -> int

- :pythonfunc:`SPI`: <anonymous>(self) -> int

- :pythonfunc:`SampleCount`: <anonymous>(self) -> int

- :pythonfunc:`Target`: <anonymous>(self) -> Target

- :pythonfunc:`Technique`: <anonymous>(self) -> str

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`clearFramebuffer(self) -> None clearFramebuffer(self, arg: str, /) -> None`



- :pythonfunc:`getFramebuffer(self, aov: str = '') -> CPUImage`



- :pythonfunc:`incFrameCount(self) -> None`



- :pythonfunc:`reset(self) -> None`



- :pythonfunc:`setCameraOrientationParameter(self, arg: {CameraOrientation}, /) -> None`



- :pythonfunc:`setParameter(self, arg0: str, arg1: int, /) -> None setParameter(self, arg0: str, arg1: float, /) -> None setParameter(self, arg0: str, arg1: Vec3, /) -> None setParameter(self, arg0: str, arg1: Vec4, /) -> None`



- :pythonfunc:`step(self, ignoreDenoiser: bool = False) -> None`



- :pythonfunc:`tonemap(self, arg: CPUArray2d_UInt32, /) -> None`



- :pythonfunc:`trace(self, arg: list[{Ray}], /) -> list[Vec3]`




.. _RuntimeOptions:

RuntimeOptions
-----------------------------------------------

Options to customize runtime behaviour

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`AcquireStats`: Set True if statistical data should be acquired while rendering

- :pythonfunc:`DumpShader`: Set True if most shader should be dumped into the filesystem

- :pythonfunc:`DumpShaderFull`: Set True if all shader should be dumped into the filesystem

- :pythonfunc:`EnableTonemapping`: Set True if any of the two tonemapping functions ``tonemap`` and ``imageinfo`` is to be used

- :pythonfunc:`OverrideCamera`: Type of camera to use instead of the one used by the scene

- :pythonfunc:`OverrideFilmSize`: Type of film size to use instead of the one used by the scene

- :pythonfunc:`OverrideTechnique`: Type of technique to use instead of the one used by the scene

- :pythonfunc:`SPI`: The requested sample per iteration. Can be 0 to set automatically

- :pythonfunc:`Seed`: Seed for the random generators

- :pythonfunc:`Target`: The target device

- :pythonfunc:`WarnUnused`: Set False if you want to ignore warnings about unused property entries


.. _RuntimeWrap:

RuntimeWrap
-----------------------------------------------

Wrapper around the runtime used for proper runtime loading and shutdown

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`instance`: <anonymous>(self) -> Runtime

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`shutdown(self) -> None`




.. _Scene:

Scene
-----------------------------------------------

Class representing a whole scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`bsdfs`: <anonymous>(self) -> dict[str, SceneObject]

- :pythonfunc:`camera`: <anonymous>(self) -> SceneObject

- :pythonfunc:`entities`: <anonymous>(self) -> dict[str, SceneObject]

- :pythonfunc:`film`: <anonymous>(self) -> SceneObject

- :pythonfunc:`lights`: <anonymous>(self) -> dict[str, SceneObject]

- :pythonfunc:`media`: <anonymous>(self) -> dict[str, SceneObject]

- :pythonfunc:`shapes`: <anonymous>(self) -> dict[str, SceneObject]

- :pythonfunc:`technique`: <anonymous>(self) -> SceneObject

- :pythonfunc:`textures`: <anonymous>(self) -> dict[str, SceneObject]

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`addBSDF(self, arg0: str, arg1: {SceneObject}, /) -> None`



- :pythonfunc:`addConstantEnvLight(self) -> None`



- :pythonfunc:`addEntity(self, arg0: str, arg1: {SceneObject}, /) -> None`



- :pythonfunc:`addFrom(self, arg: {Scene}, /) -> None`



- :pythonfunc:`addLight(self, arg0: str, arg1: {SceneObject}, /) -> None`



- :pythonfunc:`addMedium(self, arg0: str, arg1: {SceneObject}, /) -> None`



- :pythonfunc:`addShape(self, arg0: str, arg1: {SceneObject}, /) -> None`



- :pythonfunc:`addTexture(self, arg0: str, arg1: {SceneObject}, /) -> None`



- :pythonfunc:`bsdf(self, arg: str, /) -> {SceneObject}`



- :pythonfunc:`entity(self, arg: str, /) -> {SceneObject}`



- :pythonfunc:`light(self, arg: str, /) -> {SceneObject}`



- :pythonfunc:`medium(self, arg: str, /) -> {SceneObject}`



- :pythonfunc:`shape(self, arg: str, /) -> {SceneObject}`



- :pythonfunc:`texture(self, arg: str, /) -> {SceneObject}`




.. _SceneObject:

SceneObject
-----------------------------------------------

Class representing an object in the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`baseDir`: <anonymous>(self) -> os.PathLike

- :pythonfunc:`pluginType`: <anonymous>(self) -> str

- :pythonfunc:`properties`: <anonymous>(self) -> dict[str, SceneProperty]

- :pythonfunc:`type`: <anonymous>(self) -> IG::SceneObject::Type

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`hasProperty(self, arg: str, /) -> bool`



- :pythonfunc:`property(self, arg: str, /) -> {SceneProperty}`



- :pythonfunc:`setProperty(self, arg0: str, arg1: {SceneProperty}, /) -> None`




.. _SceneParser:

SceneParser
-----------------------------------------------

Parser for standard JSON and glTF scene description

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`loadFromFile(self, path: os.PathLike, flags: int = 13303) -> {Scene}`



- :pythonfunc:`loadFromString(self, str: str, opt_dir: os.PathLike = '', flags: int = 13303) -> {Scene}`




.. _SceneProperty:

SceneProperty
-----------------------------------------------

Property of an object in the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`type`: <anonymous>(self) -> IG::SceneProperty::Type

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`canBeNumber(self) -> bool`



- :pythonfunc:`getBool(self, def: bool = False) -> bool`



- :pythonfunc:`getInteger(self, def: int = 0) -> int`



- :pythonfunc:`getIntegerArray(self) -> list[int]`



- :pythonfunc:`getNumber(self, def: float = 0.0) -> float`



- :pythonfunc:`getNumberArray(self) -> list[float]`



- :pythonfunc:`getString(self, def: str = '') -> str`



- :pythonfunc:`getTransform(self, def: Mat4x4 = [[1. 0. 0. 0.] [0. 1. 0. 0.] [0. 0. 1. 0.] [0. 0. 0. 1.]]) -> Mat4x4`



- :pythonfunc:`getVector2(self, def: Vec2 = [0. 0.]) -> Vec2`



- :pythonfunc:`getVector3(self, def: Vec3 = [0. 0. 0.]) -> Vec3`



- :pythonfunc:`isValid(self) -> bool`




.. _Target:

Target
-----------------------------------------------

Target specification the runtime is using

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`CPUArchitecture`: <anonymous>(self) -> CPUArchitecture

- :pythonfunc:`Device`: <anonymous>(self) -> int

- :pythonfunc:`GPUArchitecture`: <anonymous>(self) -> GPUArchitecture

- :pythonfunc:`IsCPU`: <anonymous>(self) -> bool

- :pythonfunc:`IsGPU`: <anonymous>(self) -> bool

- :pythonfunc:`IsValid`: <anonymous>(self) -> bool

- :pythonfunc:`ThreadCount`: <anonymous>(self) -> int

- :pythonfunc:`VectorWidth`: <anonymous>(self) -> int

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`toString(self) -> str`




.. _SceneObject.Type:

SceneObject.Type
-----------------------------------------------

- :pythonfunc:`Missing Documentation`



.. _SceneParser.Flags:

SceneParser.Flags
-----------------------------------------------

- :pythonfunc:`Missing Documentation`



.. _SceneProperty.Type:

SceneProperty.Type
-----------------------------------------------

- :pythonfunc:`Missing Documentation`
