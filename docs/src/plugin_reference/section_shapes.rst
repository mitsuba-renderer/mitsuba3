.. _sec-shapes:

Shapes
======

This section presents an overview of the shape plugins that are released along with the renderer.

In Mitsuba 3, shapes define surfaces that mark transitions between different types of materials. For
instance, a shape could describe a boundary between air and a solid object, such as a piece of rock.
Alternatively, a shape can mark the beginning of a region of space that isn’t solid at all, but
rather contains a participating medium, such as smoke or steam. Finally, a shape can be used to
create an object that emits light on its own.

Shapes are usually declared along with a *BSDF* characterizing the material's
response to light (see the :ref:`respective section <sec-bsdfs>` for more
detail).

.. tabs::
    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- .. scene contents .. -->

            <shape type=".. shape type ..">
                .. shape parameters ..

                <bsdf type=".. BSDF type ..">
                    .. bsdf parameters ..
                </bsdf>

                <!-- Alternatively: reference a named BSDF that
                    has been declared previously

                    <ref id="my_bsdf"/>
                -->
            </shape>
        </scene>

    .. code-tab:: python

        'type': 'scene',

        # .. scene contents ..

        'shape_id': {
            'type': '<shape_type>',
            'bsdf_id': {
                'type': '<bsdf_type>',
                # .. bsdf parameters ..
            }

            # Alternatively, reference a named BSDF that had been declared previously
            # 'bsdf_id' : {
            #     'type' : 'ref',
            #     'id' : 'some_bsdf_id'
            # }
        }

.. _sec-shape-mesh-parameters:

Mesh parameters
---------------

Mitsuba provides several plugins that represent their geometry as a triangle
mesh: :ref:`obj <shape-obj>`, :ref:`ply <shape-ply>`, and :ref:`serialized
<shape-serialized>` load meshes from the file format of the same name, and
:ref:`cube <shape-cube>` provides a built-in box primitive. All of them are
instances of the same underlying mesh class and therefore expose an identical
set of scene parameters through :monosp:`mi.traverse()`.

The parameters are row-major tensors, with ``F`` faces, ``V`` vertices, ``P``
surface points, and ``N`` normal groups:

.. pluginparameters::

 * - faces
   - :paramtype:`TensorXu` ``(F, 3)``
   - Vertex indices of each triangle, in ``[0, V)``
   - |exposed|, |discontinuous|

 * - bsdf_index
   - :paramtype:`UInt32` ``(F,)``
   - Per-face BSDF index. An empty list indicates that all faces use BSDF entry 0.
   - |exposed|

 * - position_index
   - :paramtype:`UInt32` ``(V,)``
   - Surface point of each vertex, in ``[0, P)``. An empty list encodes the
     identity and indicates that ``P == V``.
   - |exposed|, |discontinuous|

 * - positions
   - :paramtype:`TensorXf` ``(P, 3)``
   - Surface point positions in world space
   - |exposed|, |differentiable|, |discontinuous|

 * - normal_index
   - :paramtype:`UInt32` ``(V,)``
   - Normal group of each vertex, in ``[0, N)``. An empty list encodes the
     identity and indicates that ``N == V``.
   - |exposed|

 * - normals
   - :paramtype:`TensorXf` ``(N, 3)``
   - Shading normals in world space, only present on meshes with vertex
     normals
   - |exposed|, |differentiable|, |discontinuous|

 * - texcoords
   - :paramtype:`TensorXf` ``(V, 2)``
   - Per-vertex texture coordinates, only present on meshes with a UV
     parameterization
   - |exposed|, |differentiable|

 * - (Mesh attribute)
   - :paramtype:`TensorXf` ``(V, d)`` or ``(F, d)``
   - Custom per-vertex or per-face attribute with ``d`` channels
   - |exposed|, |differentiable|

Mitsuba ``Mesh`` class can choose to store attributes (positions, normals, UVs)
at different granularities. This makes it possible to represent shapes that are
geometrically continuous while having discontinuous normals or UVs.
For example, it is relatively common for a mesh to have a crease edge where
normals jump, or separate UV patches covering the geoemtry.

The naive way to represent parameter disconinuities involves splitting vertices
so that the faces on either side get their own copy, but this also cuts the
geometry apart. This causes normal creases, discontinuities in a differentiable
renderer, and tearing under optimization.

Mitsuba uses the ``position_index`` and ``normal_index`` maps to map vertex
indices to potentially coarser position and normal indices, so that copies
split at a seam still reference the same underlying position or normal. This
sharing removes the drawbacks of naive splitting mentioned above.

Everything else (texture coordinates, tangents, and mesh attributes) is indexed
by vertex. Reading the texture coordinate of corner ``c`` of face ``f`` is
therefore a direct lookup,

.. code-block:: python

    vertex_idx = faces[f][c]
    texcoord   = texcoords[vertex_idx]

while positions and normals take one further step through their map:

.. code-block:: python

    vertex_idx   = faces[f][c]
    position_idx = position_index[vertex_idx] if len(position_index) > 0 else vertex_idx
    position     = positions[position_idx]


All parameters are writable and can may also change in shape (e.g., when
remeshing to a different mesh resolution). Following an update, Mitsuba
automatically keeps dependent state in sync, e.g., by regenerating shading
normals when the geometry changes but no new normals were provided by the user.

The following subsections discuss the available shape types in greater detail.
