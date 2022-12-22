Python API
==========

.. _Ignis (module):

Ignis (module)
-----------------------------------------------

Ignis python interface

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`flushLog() -> None`

  Flush internal logs

- :pythonfunc:`loadFromFile(*args, **kwargs)`

  Overloaded function.

  1. :pythonfunc:`loadFromFile(arg0: str) -> {RuntimeWrap}`

     Load a scene from file and generate a default runtime

  2. :pythonfunc:`loadFromFile(arg0: str, arg1: {RuntimeOptions}) -> {RuntimeWrap}`

     Load a scene from file and generate a runtime with given options

- :pythonfunc:`loadFromScene(*args, **kwargs)`

  Overloaded function.

  1. :pythonfunc:`loadFromScene(arg0: {Scene}) -> {RuntimeWrap}`

     Generate a default runtime from an already loaded scene

  2. :pythonfunc:`loadFromScene(arg0: {Scene}, arg1: str) -> {RuntimeWrap}`

     Generate a default runtime from an already loaded scene with directory for external resources

  3. :pythonfunc:`loadFromScene(arg0: {Scene}, arg1: {RuntimeOptions}) -> {RuntimeWrap}`

     Generate a runtime with given options from an already loaded scene

  4. :pythonfunc:`loadFromScene(arg0: {Scene}, arg1: str, arg2: {RuntimeOptions}) -> {RuntimeWrap}`

     Generate a runtime with given options from an already loaded scene with directory for external resources

- :pythonfunc:`loadFromString(*args, **kwargs)`

  Overloaded function.

  1. :pythonfunc:`loadFromString(arg0: str) -> {RuntimeWrap}`

     Load a scene from a string and generate a default runtime

  2. :pythonfunc:`loadFromString(arg0: str, arg1: str) -> {RuntimeWrap}`

     Load a scene from a string with directory for external resources and generate a default runtime

  3. :pythonfunc:`loadFromString(arg0: str, arg1: {RuntimeOptions}) -> {RuntimeWrap}`

     Load a scene from a string and generate a runtime with given options

  4. :pythonfunc:`loadFromString(arg0: str, arg1: str, arg2: {RuntimeOptions}) -> {RuntimeWrap}`

     Load a scene from a string with directory for external resources and generate a runtime with given options

- :pythonfunc:`saveExr(arg0: str, arg1: buffer) -> bool`

  Save an OpenEXR image to the filesystem

- :pythonfunc:`setQuiet(arg0: bool) -> None`

  Set True to disable all messages from the framework

- :pythonfunc:`setVerbose(arg0: bool) -> None`

  Set True to enable all messages from the framework, else only important messages will be shown


.. _CPUArchitecture:

CPUArchitecture
-----------------------------------------------

Enum holding supported CPU architectures

Entries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


- :pythonfunc:`ARM`


- :pythonfunc:`X86`


- :pythonfunc:`Unknown`


Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`name`: :pythonfunc:`name(self: handle) -> str`

- :pythonfunc:`value`: 


.. _CameraOrientation:

CameraOrientation
-----------------------------------------------

General camera orientation

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`Dir`: Direction the camera is facing

- :pythonfunc:`Eye`: Origin of the camera

- :pythonfunc:`Up`: Vector defining the up of the camera


.. _GPUVendor:

GPUVendor
-----------------------------------------------

Enum holding supported GPU vendors

Entries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


- :pythonfunc:`AMD`


- :pythonfunc:`Intel`


- :pythonfunc:`Nvidia`


- :pythonfunc:`Unknown`


Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`name`: :pythonfunc:`name(self: handle) -> str`

- :pythonfunc:`value`: 


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

- :pythonfunc:`Camera`: 

- :pythonfunc:`FrameCount`: 

- :pythonfunc:`FramebufferHeight`: 

- :pythonfunc:`FramebufferWidth`: 

- :pythonfunc:`InitialCameraOrientation`: 

- :pythonfunc:`IterationCount`: 

- :pythonfunc:`SPI`: 

- :pythonfunc:`SampleCount`: 

- :pythonfunc:`Target`: 

- :pythonfunc:`Technique`: 

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`clearFramebuffer(*args, **kwargs)`

  Overloaded function.

  1. :pythonfunc:`clearFramebuffer(self: {Runtime}) -> None`

  2. :pythonfunc:`clearFramebuffer(self: {Runtime}, arg0: str) -> None`

- :pythonfunc:`getFramebuffer(self: {Runtime}, aov: str = '') -> memoryview`

  

- :pythonfunc:`incFrameCount(self: {Runtime}) -> None`

  

- :pythonfunc:`reset(self: {Runtime}) -> None`

  

- :pythonfunc:`setCameraOrientationParameter(self: {Runtime}, arg0: {CameraOrientation}) -> None`

  

- :pythonfunc:`setParameter(*args, **kwargs)`

  Overloaded function.

  1. :pythonfunc:`setParameter(self: {Runtime}, arg0: str, arg1: int) -> None`

  2. :pythonfunc:`setParameter(self: {Runtime}, arg0: str, arg1: float) -> None`

  3. :pythonfunc:`setParameter(self: {Runtime}, arg0: str, arg1: numpy.ndarray[numpy.float32[3, 1]]) -> None`

  4. :pythonfunc:`setParameter(self: {Runtime}, arg0: str, arg1: numpy.ndarray[numpy.float32[4, 1]]) -> None`

- :pythonfunc:`step(self: {Runtime}, ignoreDenoiser: bool = False) -> None`

  

- :pythonfunc:`tonemap(self: {Runtime}, arg0: buffer) -> None`

  

- :pythonfunc:`trace(self: {Runtime}, arg0: List[{Ray}]) -> memoryview`

  


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

- :pythonfunc:`Target`: The target device

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`makeDefault(trace: bool = False) -> {RuntimeOptions}`

  


.. _RuntimeWrap:

RuntimeWrap
-----------------------------------------------

Wrapper around the runtime used for proper runtime loading and shutdown

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`instance`: 

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`shutdown(self: {RuntimeWrap}) -> None`

  


.. _Scene:

Scene
-----------------------------------------------

Class representing a whole scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`bsdfs`: 

- :pythonfunc:`camera`: 

- :pythonfunc:`entities`: 

- :pythonfunc:`film`: 

- :pythonfunc:`lights`: 

- :pythonfunc:`media`: 

- :pythonfunc:`shapes`: 

- :pythonfunc:`technique`: 

- :pythonfunc:`textures`: 

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`addBSDF(self: {Scene}, arg0: str, arg1: {SceneObject}) -> None`

  

- :pythonfunc:`addConstantEnvLight(self: {Scene}) -> None`

  

- :pythonfunc:`addEntity(self: {Scene}, arg0: str, arg1: {SceneObject}) -> None`

  

- :pythonfunc:`addFrom(self: {Scene}, arg0: {Scene}) -> None`

  

- :pythonfunc:`addLight(self: {Scene}, arg0: str, arg1: {SceneObject}) -> None`

  

- :pythonfunc:`addMedium(self: {Scene}, arg0: str, arg1: {SceneObject}) -> None`

  

- :pythonfunc:`addShape(self: {Scene}, arg0: str, arg1: {SceneObject}) -> None`

  

- :pythonfunc:`addTexture(self: {Scene}, arg0: str, arg1: {SceneObject}) -> None`

  

- :pythonfunc:`bsdf(self: {Scene}, arg0: str) -> {SceneObject}`

  

- :pythonfunc:`entity(self: {Scene}, arg0: str) -> {SceneObject}`

  

- :pythonfunc:`light(self: {Scene}, arg0: str) -> {SceneObject}`

  

- :pythonfunc:`medium(self: {Scene}, arg0: str) -> {SceneObject}`

  

- :pythonfunc:`shape(self: {Scene}, arg0: str) -> {SceneObject}`

  

- :pythonfunc:`texture(self: {Scene}, arg0: str) -> {SceneObject}`

  


.. _SceneObject:

SceneObject
-----------------------------------------------

Class representing an object in the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`baseDir`: 

- :pythonfunc:`pluginType`: 

- :pythonfunc:`properties`: 

- :pythonfunc:`type`: 

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`hasProperty(self: {SceneObject}, arg0: str) -> bool`

  

- :pythonfunc:`property(self: {SceneObject}, arg0: str) -> {SceneProperty}`

  

- :pythonfunc:`setProperty(self: {SceneObject}, arg0: str, arg1: {SceneProperty}) -> None`

  


.. _SceneParser:

SceneParser
-----------------------------------------------

Parser for standard JSON and glTF scene description

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`loadFromFile(self: {SceneParser}, path: str, flags: int = 13303) -> {Scene}`

  

- :pythonfunc:`loadFromString(self: {SceneParser}, str: str, opt_dir: str = '', flags: int = 13303) -> {Scene}`

  


.. _SceneProperty:

SceneProperty
-----------------------------------------------

Property of an object in the scene

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`type`: 

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`canBeNumber(self: {SceneProperty}) -> bool`

  

- :pythonfunc:`fromBool(arg0: bool) -> {SceneProperty}`

  

- :pythonfunc:`fromInteger(arg0: int) -> {SceneProperty}`

  

- :pythonfunc:`fromIntegerArray(arg0: List[int]) -> {SceneProperty}`

  

- :pythonfunc:`fromNumber(arg0: float) -> {SceneProperty}`

  

- :pythonfunc:`fromNumberArray(arg0: List[float]) -> {SceneProperty}`

  

- :pythonfunc:`fromString(arg0: str) -> {SceneProperty}`

  

- :pythonfunc:`fromTransform(arg0: numpy.ndarray[numpy.float32[4, 4]]) -> {SceneProperty}`

  

- :pythonfunc:`fromVector2(arg0: numpy.ndarray[numpy.float32[2, 1]]) -> {SceneProperty}`

  

- :pythonfunc:`fromVector3(arg0: numpy.ndarray[numpy.float32[3, 1]]) -> {SceneProperty}`

  

- :pythonfunc:`getBool(self: {SceneProperty}, def: bool = False) -> bool`

  

- :pythonfunc:`getInteger(self: {SceneProperty}, def: int = 0) -> int`

  

- :pythonfunc:`getIntegerArray(self: {SceneProperty}) -> List[int]`

  

- :pythonfunc:`getNumber(self: {SceneProperty}, def: float = 0.0) -> float`

  

- :pythonfunc:`getNumberArray(self: {SceneProperty}) -> List[float]`

  

- :pythonfunc:`getString(self: {SceneProperty}, def: str = '') -> str`

  

- :pythonfunc:`getTransform(self: {SceneProperty}, def: numpy.ndarray[numpy.float32[4, 4]] = array([[1., 0., 0., 0.], [0., 1., 0., 0.], [0., 0., 1., 0.], [0., 0., 0., 1.]], dtype=float32)) -> numpy.ndarray[numpy.float32[4, 4]]`

  

- :pythonfunc:`getVector2(self: {SceneProperty}, def: numpy.ndarray[numpy.float32[2, 1]] = array([0., 0.], dtype=float32)) -> numpy.ndarray[numpy.float32[2, 1]]`

  

- :pythonfunc:`getVector3(self: {SceneProperty}, def: numpy.ndarray[numpy.float32[3, 1]] = array([0., 0., 0.], dtype=float32)) -> numpy.ndarray[numpy.float32[3, 1]]`

  

- :pythonfunc:`isValid(self: {SceneProperty}) -> bool`

  


.. _Target:

Target
-----------------------------------------------

Target specification the runtime is using

Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`CPUArchitecture`: 

- :pythonfunc:`Device`: 

- :pythonfunc:`GPUVendor`: 

- :pythonfunc:`IsCPU`: 

- :pythonfunc:`IsGPU`: 

- :pythonfunc:`IsValid`: 

- :pythonfunc:`ThreadCount`: 

- :pythonfunc:`VectorWidth`: 

Methods
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`makeCPU(arg0: int, arg1: int) -> {Target}`

  

- :pythonfunc:`makeGPU(arg0: {GPUVendor}, arg1: int) -> {Target}`

  

- :pythonfunc:`makeGeneric() -> {Target}`

  

- :pythonfunc:`makeSingle() -> {Target}`

  

- :pythonfunc:`pickBest() -> {Target}`

  

- :pythonfunc:`pickCPU() -> {Target}`

  

- :pythonfunc:`pickGPU(device: int = 0) -> {Target}`

  

- :pythonfunc:`toString(self: {Target}) -> str`

  


.. _SceneObject.Type:

SceneObject.Type
-----------------------------------------------

Enum holding type of scene object

Entries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


- :pythonfunc:`Bsdf`


- :pythonfunc:`Camera`


- :pythonfunc:`Entity`


- :pythonfunc:`Film`


- :pythonfunc:`Light`


- :pythonfunc:`Medium`


- :pythonfunc:`Shape`


- :pythonfunc:`Technique`


- :pythonfunc:`Texture`


Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`name`: :pythonfunc:`name(self: handle) -> str`

- :pythonfunc:`value`: 


.. _SceneParser.Flags:

SceneParser.Flags
-----------------------------------------------

Flags modifying parsing behaviour and allowing partial scene loads

Entries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


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


Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`name`: :pythonfunc:`name(self: handle) -> str`

- :pythonfunc:`value`: 


.. _SceneProperty.Type:

SceneProperty.Type
-----------------------------------------------

Enum holding type of scene property

Entries
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


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


Properties
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- :pythonfunc:`name`: :pythonfunc:`name(self: handle) -> str`

- :pythonfunc:`value`:
