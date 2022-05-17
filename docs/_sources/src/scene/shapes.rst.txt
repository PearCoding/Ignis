Shapes
======

Shapes represent geometry in the scene and are a group of triangles. No exact primitives representing e.g., a sphere are possible in Ignis currently. This might change in the future.

A shape is specified in the :monosp:`shapes` block with a :monosp:`name` and a :monosp:`type`.
The type has to be one of the shape listed at this section below.

.. code-block:: javascript
    
    {
        // ...
        "shape": [
            // ...
            {"name":"NAME", "type":"TYPE", /* DEPENDS ON TYPE */},
            // ...
        ]
        // ...
    }

.. _shape-triangle:

Triangle (:monosp:`triangle`)
-----------------------------

.. objectparameters::

 * - p0, p1, p2
   - |vector|
   - (0,0,0), (1,0,0), (0,1,0)
   - Vertices of the triangle.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-rectangle:

Rectangle (:monosp:`rectangle`)
-------------------------------

.. objectparameters::

 * - width, height
   - |number|
   - 2, 2
   - Width and height of the rectangle in the XY-Plane. The rectangle is centered around the parameter :monosp:`origin`.

 * - origin
   - |vector|
   - (0,0,0)
   - The origin of the rectangle.

 * - p0, p1, p2, p3
   - |vector|
   - (-1,-1,0), (1,-1,0), (1,1,0), (-1,1,0)
   - Vertices of the rectangle. This will only be used if no :monosp:`width` or :monosp:`height` is specified.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-box:

Box (:monosp:`box`)
-------------------

.. objectparameters::

 * - width, height, depth
   - |number|
   - 2, 2, 2
   - Width (x-axis), height (y-axis) and depth (z-axis) of the box. The box is centered around the parameter :monosp:`origin`.

 * - origin
   - |vector|
   - (0,0,0)
   - The origin of the box.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-icosphere:

Ico-Sphere (:monosp:`icosphere`, :monosp:`sphere`)
--------------------------------------------------

.. objectparameters::

 * - radius
   - |number|
   - 1
   - Radius of the sphere.

 * - center
   - |vector|
   - (0,0,0)
   - The origin of the box.

 * - subdivions
   - |int|
   - 4
   - Number of subdivions used.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-uvsphere:

UV-Sphere (:monosp:`uvsphere`)
------------------------------

.. objectparameters::

 * - radius
   - |number|
   - 1
   - Radius of the sphere.

 * - center
   - |vector|
   - (0,0,0)
   - The origin of the box.

 * - stacks, slices
   - |int|
   - 32, 16
   - Stacks and slices used for internal triangulation.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-cylinder:

Cylinder (:monosp:`cylinder`)
-----------------------------

.. objectparameters::

 * - radius
   - |number|
   - 1
   - Radius of the cylinder for the top and bottom part.

 * - top_radius, bottom_radius
   - |number|
   - 1, 1
   - Radius of the cylinder for the top and bottom part respectively. Can not be used together with :monosp:`radius`.

 * - p0, p1
   - |vector|
   - (0,0,0), (0,0,1)
   - The origin of the top and bottom of the cylinder respectively.

 * - filled
   - |bool|
   - true
   - Set true to fill the top and bottom of the cylinder.

 * - sections
   - |int|
   - 32
   - Sections used for internal triangulation.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-cone:

Cone (:monosp:`cone`)
---------------------

.. objectparameters::

 * - radius
   - |number|
   - 1
   - Radius of the cone.

 * - p0, p1
   - |vector|
   - (0,0,0), (0,0,1)
   - The origin of the top and bottom of the cone respectively.

 * - filled
   - |bool|
   - true
   - Set true to fill the bottom of the cone.

 * - sections
   - |int|
   - 32
   - Sections used for internal triangulation.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-disk:

Disk (:monosp:`disk`)
---------------------

.. objectparameters::

 * - radius
   - |number|
   - 1
   - Radius of the disk.

 * - origin
   - |vector|
   - (0,0,0)
   - The origin of the disk.

 * - normal
   - |vector|
   - (0,0,1)
   - The normal of the disk.

 * - sections
   - |int|
   - 32
   - Sections used for internal triangulation.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-obj:

Wavefront Object Format (:monosp:`obj`)
---------------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid .obj file.

 * - shape_index
   - |int|
   - -1
   - If greater or equal 0 a specific shape given by the index will be loaded, else all shapes will be merged to one.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-ply:

Polygon File Format (:monosp:`ply`)
-----------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid .ply file.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.

.. _shape-mitsuba:

Mitsuba Serialized Format (:monosp:`mitsuba`)
---------------------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid .serialized file.

 * - shape_index
   - |int|
   - 0
   - A Mitsuba Serialized Format is able to contain multiple shapes. This parameter allows to select the requested one.

 * - flip_normals
   - |bool|
   - false
   - Flip the normals.

 * - face_normals
   - |bool|
   - false
   - Use normals from triangles as vertex normals. This will let the object look *hard*.

 * - transform
   - |transform|
   - Identity
   - Apply given transformation to shape.