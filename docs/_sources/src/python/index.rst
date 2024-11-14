Python API
==========

.. _Ignis (module):

Ignis (module)
-----------------------------------------------

Ignis python interface

.. _Ignis (module)-enums:

Enums
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _CPUArchitecture:

- :pythonfunc:`CPUArchitecture`:

  Enum holding supported CPU architectures


  - :pythonfunc:`ARM`
  - :pythonfunc:`X86`
  - :pythonfunc:`Unknown`

.. _GPUArchitecture:

- :pythonfunc:`GPUArchitecture`:

  Enum holding supported GPU architectures


  - :pythonfunc:`AMD`
  - :pythonfunc:`Intel`
  - :pythonfunc:`Nvidia`
  - :pythonfunc:`Unknown`

.. _LogLevel:

- :pythonfunc:`LogLevel`:

  Enum holding verbosity level for logging


  - :pythonfunc:`Debug`
  - :pythonfunc:`Info`
  - :pythonfunc:`Warning`
  - :pythonfunc:`Error`
  - :pythonfunc:`Fatal`



.. _Ignis (module)-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`flushLog() -> None`:

  Flush internal logs

- :pythonfunc:`loadFromFile(arg: str | os.PathLike) -> {RuntimeWrap}`:

  Load a scene from file and generate a default runtime

- :pythonfunc:`loadFromFile(arg0: str | os.PathLike, arg1: {RuntimeOptions}) -> {RuntimeWrap}`:

  Load a scene from file and generate a runtime with given options

- :pythonfunc:`loadFromScene(arg: {Scene}) -> {RuntimeWrap}`:

  Generate a default runtime from an already loaded scene

- :pythonfunc:`loadFromScene(arg0: {Scene}, arg1: str | os.PathLike) -> {RuntimeWrap}`:

  Generate a default runtime from an already loaded scene with directory for external resources

- :pythonfunc:`loadFromScene(arg0: {Scene}, arg1: {RuntimeOptions}) -> {RuntimeWrap}`:

  Generate a runtime with given options from an already loaded scene

- :pythonfunc:`loadFromScene(arg0: {Scene}, arg1: str | os.PathLike, arg2: {RuntimeOptions}) -> {RuntimeWrap}`:

  Generate a runtime with given options from an already loaded scene with directory for external resources

- :pythonfunc:`loadFromString(arg: str) -> {RuntimeWrap}`:

  Load a scene from a string and generate a default runtime

- :pythonfunc:`loadFromString(arg0: str, arg1: str | os.PathLike) -> {RuntimeWrap}`:

  Load a scene from a string with directory for external resources and generate a default runtime

- :pythonfunc:`loadFromString(arg0: str, arg1: {RuntimeOptions}) -> {RuntimeWrap}`:

  Load a scene from a string and generate a runtime with given options

- :pythonfunc:`loadFromString(arg0: str, arg1: str | os.PathLike, arg2: {RuntimeOptions}) -> {RuntimeWrap}`:

  Load a scene from a string with directory for external resources and generate a runtime with given options

- :pythonfunc:`saveExr(arg0: str | os.PathLike, arg1: CPUImage) -> bool`:

  Save an OpenEXR image to the filesystem

- :pythonfunc:`saveExr(arg0: str | os.PathLike, arg1: CPUArray2d_Float32) -> bool`:

  Save an OpenEXR grayscale image to the filesystem

- :pythonfunc:`setQuiet(arg: bool) -> None`:

  Set True to disable all messages from the framework

- :pythonfunc:`setVerbose(arg: bool) -> None`:

  Set True to enable all messages from the framework, else only important messages will be shown. Shortcut for setVerbose(LogLevel_.Debug)

- :pythonfunc:`setVerbose(arg: {LogLevel}) -> None`:

  Set the level of verbosity for the logger



.. _BoundingBox:

BoundingBox
-----------------------------------------------

BoundingBox

.. _BoundingBox-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Center`:

  Returns :pythonfunc:`Vec3`

- :pythonfunc:`Diameter`:

  Returns :pythonfunc:`Vec3`

- :pythonfunc:`HalfArea`:

  Returns :pythonfunc:`float`

- :pythonfunc:`IsEmpty`:

  Returns :pythonfunc:`bool`

- :pythonfunc:`Max`:

  Returns :pythonfunc:`Vec3`

- :pythonfunc:`Min`:

  Returns :pythonfunc:`Vec3`

- :pythonfunc:`Volume`:

  Returns :pythonfunc:`float`



.. _BoundingBox-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`MakeEmpty() -> {BoundingBox}`:

  *No documentation*

- :pythonfunc:`MakeFull() -> {BoundingBox}`:

  *No documentation*

- :pythonfunc:`extend(self, arg: {BoundingBox}) -> {BoundingBox}`:

  *No documentation*

- :pythonfunc:`extend(self, arg: Vec3) -> {BoundingBox}`:

  *No documentation*

- :pythonfunc:`inflate(self, arg: float) -> None`:

  *No documentation*

- :pythonfunc:`isInside(self, arg: Vec3) -> bool`:

  *No documentation*

- :pythonfunc:`isOverlapping(self, arg: {BoundingBox}) -> bool`:

  *No documentation*

- :pythonfunc:`overlap(self, arg: {BoundingBox}) -> {BoundingBox}`:

  *No documentation*



.. _CameraOrientation:

CameraOrientation
-----------------------------------------------

General camera orientation

.. _CameraOrientation-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Dir`:

  Direction the camera is facing

- :pythonfunc:`Eye`:

  Origin of the camera

- :pythonfunc:`Up`:

  Vector defining the up of the camera



.. _DenoiserSettings:

DenoiserSettings
-----------------------------------------------

Settings for the denoiser

.. _DenoiserSettings-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Enabled`:

  Enable or disable the denoiser

- :pythonfunc:`HighQuality`:

  Set True if denoiser should be high quality or interactive

- :pythonfunc:`Prefilter`:

  Set True if normal and albedo layer should be prefiltered



.. _Ray:

Ray
-----------------------------------------------

Single ray traced into the scene

.. _Ray-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Direction`:

  Direction of the ray

- :pythonfunc:`Origin`:

  Origin of the ray

- :pythonfunc:`Range`:

  Range (tmin, tmax) of the ray



.. _Runtime:

Runtime
-----------------------------------------------

Renderer runtime allowing control of simulation and access to results

.. _Runtime-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`AOVs`:

  Returns :pythonfunc:`list[str]`

- :pythonfunc:`Camera`:

  Returns :pythonfunc:`str`

- :pythonfunc:`ColorParameters`:

  Returns :pythonfunc:`dict[str, Vec4]`

- :pythonfunc:`FloatParameters`:

  Returns :pythonfunc:`dict[str, float]`

- :pythonfunc:`FrameCount`:

  Returns :pythonfunc:`int`

- :pythonfunc:`FramebufferHeight`:

  Returns :pythonfunc:`int`

- :pythonfunc:`FramebufferWidth`:

  Returns :pythonfunc:`int`

- :pythonfunc:`InitialCameraOrientation`:

  Returns :pythonfunc:`{CameraOrientation}`

- :pythonfunc:`IntParameters`:

  Returns :pythonfunc:`dict[str, int]`

- :pythonfunc:`IterationCount`:

  Returns :pythonfunc:`int`

- :pythonfunc:`RenderStartTime`:

  Returns :pythonfunc:`int`

- :pythonfunc:`SPI`:

  Returns :pythonfunc:`int`

- :pythonfunc:`SampleCount`:

  Returns :pythonfunc:`int`

- :pythonfunc:`SceneBoundingBox`:

  Returns :pythonfunc:`{BoundingBox}`

- :pythonfunc:`Seed`:

  Returns :pythonfunc:`int`

- :pythonfunc:`StringParameters`:

  Returns :pythonfunc:`dict[str, str]`

- :pythonfunc:`Target`:

  Returns :pythonfunc:`{Target}`

- :pythonfunc:`Technique`:

  Returns :pythonfunc:`str`

- :pythonfunc:`VectorParameters`:

  Returns :pythonfunc:`dict[str, Vec3]`



.. _Runtime-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`clearFramebuffer(self) -> None`:

  *No documentation*

- :pythonfunc:`clearFramebuffer(self, arg: str) -> None`:

  *No documentation*

- :pythonfunc:`getFramebufferForDevice(self, aov: str = '') -> Image`:

  *No documentation*

- :pythonfunc:`getFramebufferForHost(self, aov: str = '') -> CPUImage`:

  *No documentation*

- :pythonfunc:`incFrameCount(self) -> None`:

  *No documentation*

- :pythonfunc:`reset(self) -> None`:

  Reset internal counters etc. This should be used if data (like camera orientation) has changed. Frame counter will NOT be reset

- :pythonfunc:`saveFramebuffer(self, arg: str | os.PathLike) -> bool`:

  *No documentation*

- :pythonfunc:`setCameraOrientation(self, arg: {CameraOrientation}) -> None`:

  *No documentation*

- :pythonfunc:`setParameter(self, arg0: str, arg1: int) -> None`:

  *No documentation*

- :pythonfunc:`setParameter(self, arg0: str, arg1: float) -> None`:

  *No documentation*

- :pythonfunc:`setParameter(self, arg0: str, arg1: Vec3) -> None`:

  *No documentation*

- :pythonfunc:`setParameter(self, arg0: str, arg1: Vec4) -> None`:

  *No documentation*

- :pythonfunc:`setParameter(self, arg0: str, arg1: str) -> None`:

  *No documentation*

- :pythonfunc:`step(self, ignoreDenoiser: bool = False) -> None`:

  *No documentation*

- :pythonfunc:`tonemap(self, arg: CPUArray2d_UInt32) -> None`:

  *No documentation*

- :pythonfunc:`trace(self, arg: list[{Ray}]) -> list[Vec3]`:

  *No documentation*



.. _RuntimeOptions:

RuntimeOptions
-----------------------------------------------

Options to customize runtime behaviour

.. _RuntimeOptions-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`AcquireStats`:

  Set True if statistical data should be acquired while rendering

- :pythonfunc:`Denoiser`:

  Settings for the denoiser

- :pythonfunc:`DumpShader`:

  Set True if most shader should be dumped into the filesystem

- :pythonfunc:`DumpShaderFull`:

  Set True if all shader should be dumped into the filesystem

- :pythonfunc:`EnableTonemapping`:

  Set True if any of the two tonemapping functions ``tonemap`` and ``imageinfo`` is to be used

- :pythonfunc:`OverrideCamera`:

  Type of camera to use instead of the one used by the scene

- :pythonfunc:`OverrideFilmSize`:

  Type of film size to use instead of the one used by the scene

- :pythonfunc:`OverrideTechnique`:

  Type of technique to use instead of the one used by the scene

- :pythonfunc:`SPI`:

  The requested sample per iteration. Can be 0 to set automatically

- :pythonfunc:`Seed`:

  Seed for the random generators

- :pythonfunc:`Target`:

  The target device

- :pythonfunc:`WarnUnused`:

  Set False if you want to ignore warnings about unused property entries



.. _RuntimeOptions-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`makeDefault(trace: bool = False) -> {RuntimeOptions}`:

  *No documentation*



.. _RuntimeWrap:

RuntimeWrap
-----------------------------------------------

Wrapper around the runtime used for proper runtime loading and shutdown

.. _RuntimeWrap-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`instance`:

  Returns :pythonfunc:`{Runtime}`



.. _RuntimeWrap-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`shutdown(self) -> None`:

  *No documentation*



.. _Scene:

Scene
-----------------------------------------------

Class representing a whole scene

.. _Scene-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`bsdfs`:

  Returns :pythonfunc:`dict[str, {SceneObject}]`

- :pythonfunc:`camera`:

  Returns :pythonfunc:`{SceneObject}`

- :pythonfunc:`entities`:

  Returns :pythonfunc:`dict[str, {SceneObject}]`

- :pythonfunc:`film`:

  Returns :pythonfunc:`{SceneObject}`

- :pythonfunc:`lights`:

  Returns :pythonfunc:`dict[str, {SceneObject}]`

- :pythonfunc:`media`:

  Returns :pythonfunc:`dict[str, {SceneObject}]`

- :pythonfunc:`shapes`:

  Returns :pythonfunc:`dict[str, {SceneObject}]`

- :pythonfunc:`technique`:

  Returns :pythonfunc:`{SceneObject}`

- :pythonfunc:`textures`:

  Returns :pythonfunc:`dict[str, {SceneObject}]`



.. _Scene-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`addBSDF(self, arg0: str, arg1: {SceneObject}) -> None`:

  *No documentation*

- :pythonfunc:`addConstantEnvLight(self) -> None`:

  *No documentation*

- :pythonfunc:`addEntity(self, arg0: str, arg1: {SceneObject}) -> None`:

  *No documentation*

- :pythonfunc:`addFrom(self, arg: {Scene}) -> None`:

  *No documentation*

- :pythonfunc:`addLight(self, arg0: str, arg1: {SceneObject}) -> None`:

  *No documentation*

- :pythonfunc:`addMedium(self, arg0: str, arg1: {SceneObject}) -> None`:

  *No documentation*

- :pythonfunc:`addShape(self, arg0: str, arg1: {SceneObject}) -> None`:

  *No documentation*

- :pythonfunc:`addTexture(self, arg0: str, arg1: {SceneObject}) -> None`:

  *No documentation*

- :pythonfunc:`bsdf(self, arg: str) -> {SceneObject}`:

  *No documentation*

- :pythonfunc:`entity(self, arg: str) -> {SceneObject}`:

  *No documentation*

- :pythonfunc:`light(self, arg: str) -> {SceneObject}`:

  *No documentation*

- :pythonfunc:`loadFromFile(path: str | os.PathLike, flags: int = 13303) -> {Scene}`:

  *No documentation*

- :pythonfunc:`loadFromString(str: str, opt_dir: str | os.PathLike = '', flags: int = 13303) -> {Scene}`:

  *No documentation*

- :pythonfunc:`medium(self, arg: str) -> {SceneObject}`:

  *No documentation*

- :pythonfunc:`shape(self, arg: str) -> {SceneObject}`:

  *No documentation*

- :pythonfunc:`texture(self, arg: str) -> {SceneObject}`:

  *No documentation*



.. _SceneObject:

SceneObject
-----------------------------------------------

Class representing an object in the scene

.. _SceneObject-enums:

Enums
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _SceneObject-Type:

- :pythonfunc:`Type`:

  Enum holding type of scene object


  - :pythonfunc:`Bsdf`
  - :pythonfunc:`Camera`
  - :pythonfunc:`Entity`
  - :pythonfunc:`Film`
  - :pythonfunc:`Light`
  - :pythonfunc:`Medium`
  - :pythonfunc:`Shape`
  - :pythonfunc:`Technique`
  - :pythonfunc:`Texture`



.. _SceneObject-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`baseDir`:

  Returns :pythonfunc:`pathlib.Path`

- :pythonfunc:`pluginType`:

  Returns :pythonfunc:`str`

- :pythonfunc:`properties`:

  Returns :pythonfunc:`dict[str, {SceneProperty}]`

- :pythonfunc:`type`:

  Returns :pythonfunc:`{SceneObject.Type}`



.. _SceneObject-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`hasProperty(self, arg: str) -> bool`:

  *No documentation*

- :pythonfunc:`property(self, arg: str) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`setProperty(self, arg0: str, arg1: {SceneProperty}) -> None`:

  *No documentation*



.. _SceneParser:

SceneParser
-----------------------------------------------

Parser for standard JSON and glTF scene description

.. _SceneParser-enums:

Enums
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _SceneParser-Flags:

- :pythonfunc:`Flags`:

  Flags modifying parsing behaviour and allowing partial scene loads


  - :pythonfunc:`F_LoadCamera`
  - :pythonfunc:`F_LoadFilm`
  - :pythonfunc:`F_LoadTechnique`
  - :pythonfunc:`F_LoadBSDFs`
  - :pythonfunc:`F_LoadMedia`
  - :pythonfunc:`F_LoadLights`
  - :pythonfunc:`F_LoadTextures`
  - :pythonfunc:`F_LoadShapes`
  - :pythonfunc:`F_LoadEntities`
  - :pythonfunc:`F_LoadExternals`
  - :pythonfunc:`F_LoadAll`



.. _SceneParser-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`loadFromFile(self, path: str | os.PathLike, flags: int = 13303) -> {Scene}`:

  *No documentation*

- :pythonfunc:`loadFromString(self, str: str, opt_dir: str | os.PathLike = '', flags: int = 13303) -> {Scene}`:

  *No documentation*



.. _SceneProperty:

SceneProperty
-----------------------------------------------

Property of an object in the scene

.. _SceneProperty-enums:

Enums
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. _SceneProperty-Type:

- :pythonfunc:`Type`:

  Enum holding type of scene property


  - :pythonfunc:`None`
  - :pythonfunc:`Bool`
  - :pythonfunc:`Integer`
  - :pythonfunc:`Number`
  - :pythonfunc:`String`
  - :pythonfunc:`Transform`
  - :pythonfunc:`Vector2`
  - :pythonfunc:`Vector3`
  - :pythonfunc:`IntegerArray`
  - :pythonfunc:`NumberArray`



.. _SceneProperty-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`type`:

  Returns :pythonfunc:`{SceneProperty.Type}`



.. _SceneProperty-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`canBeNumber(self) -> bool`:

  *No documentation*

- :pythonfunc:`fromBool(arg: bool) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromInteger(arg: int) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromIntegerArray(arg: list[int]) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromNumber(arg: float) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromNumberArray(arg: list[float]) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromString(arg: str) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromTransform(arg: Mat4x4) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromVector2(arg: Vec2) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`fromVector3(arg: Vec3) -> {SceneProperty}`:

  *No documentation*

- :pythonfunc:`getBool(self, def: bool = False) -> bool`:

  *No documentation*

- :pythonfunc:`getInteger(self, def: int = 0) -> int`:

  *No documentation*

- :pythonfunc:`getIntegerArray(self) -> list[int]`:

  *No documentation*

- :pythonfunc:`getNumber(self, def: float = 0.0) -> float`:

  *No documentation*

- :pythonfunc:`getNumberArray(self) -> list[float]`:

  *No documentation*

- :pythonfunc:`getString(self, def: str = '') -> str`:

  *No documentation*

- :pythonfunc:`getTransform(self, def: Mat4x4 = Mat4x4.Identity -> Mat4x4`:

  *No documentation*

- :pythonfunc:`getVector2(self, def: Vec2 = Vec2(0)) -> Vec2`:

  *No documentation*

- :pythonfunc:`getVector3(self, def: Vec3 = Vec3(0)) -> Vec3`:

  *No documentation*

- :pythonfunc:`isValid(self) -> bool`:

  *No documentation*



.. _Target:

Target
-----------------------------------------------

Target specification the runtime is using

.. _Target-properties:

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Architecture`:

  Returns :pythonfunc:`{CPUArchitecture} | {GPUArchitecture}`

- :pythonfunc:`CPUArchitecture`:

  Returns :pythonfunc:`{CPUArchitecture}`

- :pythonfunc:`Device`:

  Returns :pythonfunc:`int`

- :pythonfunc:`GPUArchitecture`:

  Returns :pythonfunc:`{GPUArchitecture}`

- :pythonfunc:`IsCPU`:

  Returns :pythonfunc:`bool`

- :pythonfunc:`IsGPU`:

  Returns :pythonfunc:`bool`

- :pythonfunc:`IsValid`:

  Returns :pythonfunc:`bool`

- :pythonfunc:`ThreadCount`:

  Returns :pythonfunc:`int`

- :pythonfunc:`VectorWidth`:

  Returns :pythonfunc:`int`



.. _Target-methods:

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`makeCPU(arg0: int, arg1: int) -> {Target}`:

  *No documentation*

- :pythonfunc:`makeGPU(arg0: {GPUArchitecture}, arg1: int) -> {Target}`:

  *No documentation*

- :pythonfunc:`makeGeneric() -> {Target}`:

  *No documentation*

- :pythonfunc:`makeSingle() -> {Target}`:

  *No documentation*

- :pythonfunc:`pickBest() -> {Target}`:

  *No documentation*

- :pythonfunc:`pickCPU() -> {Target}`:

  *No documentation*

- :pythonfunc:`pickGPU(device: int = 0) -> {Target}`:

  *No documentation*

- :pythonfunc:`toString(self) -> str`:

  *No documentation*




