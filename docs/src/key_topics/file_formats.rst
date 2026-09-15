.. _sec-file-formats:

File formats
============

Besides standard image and mesh formats, Mitsuba reads two file formats of
its own that are specified here.

- The :ref:`scene XML format <sec-file-format>` describes which plugins to
  instantiate and how to connect them.

- The :ref:`packed container format <sec-packed-format>` described below
  stores meshes and textures in the memory layout of the renderer.

.. _sec-packed-format:

Packed container (``.packed``)
------------------------------

A ``.packed`` file bundles the meshes and textures of a scene in a single
file. Its entries store data in the internal representation of the renderer,
so that a reader can decompress the file contents straight into device
buffers without decoding or format conversion steps. The :ref:`packed
<shape-packed>` shape plugin and the :ref:`bitmap <texture-bitmap>` texture
plugin each load one entry of a container, selected by its position
(``index``) or its ``name``. The :py:class:`mitsuba.PackedFile` class
implements reading and writing.

The file consists of a header, the data of all entries, and a dictionary
that locates them. The format is designed to be written in a streaming
fashion, while allowing random access to its contents. All fields use little
endian byte order.

.. list-table::
    :widths: 20 80
    :header-rows: 1

    * - Type
      - Content
    * - :monosp:`char[8]`
      - The ASCII bytes ``MIPACKED``
    * - :monosp:`uint16`
      - File version, currently 1
    * - :monosp:`uint16`
      - The number of entries :math:`n`
    * - :monosp:`uint32`
      - Unused, zero
    * - :monosp:`uint64`
      - File offset of the dictionary

The data of the entries follows the header. The dictionary at the given
offset consists of :math:`n` records with the following fields:

.. list-table::
    :widths: 20 80
    :header-rows: 1

    * - Type
      - Content
    * - :monosp:`uint64`
      - File offset of the entry's data
    * - :monosp:`uint64`
      - Size of the data in bytes
    * - :monosp:`uint32`
      - Offset of the entry's name within the name table
    * - :monosp:`uint32`
      - Length of the name in bytes

The name table with the UTF-8 bytes of all names, without separators,
concludes the file.

.. _sec-packed-array:

Compressed arrays
*****************

Every entry consists of an uncompressed header followed by one or more
compressed arrays, which hold the bulk data. An array is split into blocks
of 16 MiB (the last block is shorter) that are individually compressed with
LZ4-HC, so that a reader can decompress each block straight into its final
location. The size of the array is stored ahead of the blocks, which lets the
reader validate the expected size before allocating memory.

.. list-table::
    :widths: 20 80
    :header-rows: 1

    * - Type
      - Content
    * - :monosp:`uint64`
      - Uncompressed size of the array in bytes
    * - :monosp:`uint32`
      - Uncompressed size of each block, currently 16 MiB
    * - :monosp:`uint32`
      - Compressed size of the first block
    * - :monosp:`array`
      - The LZ4 block (the block format without a frame header)
    * - :math:`\cdots`
      - Compressed size and data of the remaining blocks

.. _sec-packed-mesh:

Mesh entries
************

A mesh entry stores the packed vertex and face records of the
:py:class:`mitsuba.Mesh` class, and :py:meth:`mitsuba.Mesh.write_packed`
produces it. The entry begins with the following header:

.. list-table::
    :widths: 20 80
    :header-rows: 1

    * - Type
      - Content
    * - :monosp:`char[4]`
      - The ASCII bytes ``MESH``
    * - :monosp:`uint32`
      - Entry version, currently 1
    * - :monosp:`uint32`
      - Flags, see below
    * - :monosp:`uint32`
      - Number of vertices ``V``
    * - :monosp:`uint32`
      - Number of triangles ``F``
    * - :monosp:`uint32`
      - Number of distinct surface points ``P``, or 0 when the
        vertex-to-surface-point map is the identity and not stored
    * - :monosp:`uint32`
      - Number of normal groups ``N``, or 0 when the vertex-to-normal-group
        map is the identity and not stored
    * - :monosp:`uint32`
      - Number of custom mesh attributes
    * - :monosp:`attribute`
      - One header per custom mesh attribute, see below

The flag word combines the following bits:

.. list-table::
    :widths: 20 80
    :header-rows: 1

    * - Bit
      - Meaning
    * - :code:`0x1`
      - The vertex records store shading normals
    * - :code:`0x2`
      - The vertex records store shading tangents. The frame lanes then hold
        the complete encoded shading frame instead of the normal.
    * - :code:`0x4`
      - The vertex records store texture coordinates
    * - :code:`0x10`
      - Use face normals instead of smoothly interpolated vertex normals

Each attribute header consists of the following fields:

.. list-table::
    :widths: 20 80
    :header-rows: 1

    * - Type
      - Content
    * - :monosp:`uint32`
      - Length of the attribute name in bytes
    * - :monosp:`char[]`
      - The name in UTF-8. Its ``vertex_`` or ``face_`` prefix selects the
        domain of the attribute.
    * - :monosp:`uint8`
      - Flags. Bit 0 marks values that a spectral variant wrote as
        sRGB-to-spectrum upsampling coefficients rather than raw values.
    * - :monosp:`uint8`
      - Channel count ``dim`` in [1, 4]

The header is followed by these compressed arrays, in order:

- ``8 V`` single precision floats: the packed vertex records (position in
  lanes 0-2, the shading normal or, with stored tangents, the encoded shading
  frame in lanes 3-5, texture coordinates in lanes 6-7; unused lanes are zero).

- ``4 F`` :monosp:`uint32` face records: three vertex indices and the
  per-face BSDF index.

- ``V`` :monosp:`uint32` vertex-to-surface-point indices in ``[0, P)``.
  Omitted when ``P`` is 0.

- ``V`` :monosp:`uint32` vertex-to-normal-group indices in ``[0, N)``.
  Omitted when ``N`` is 0.

- Per custom attribute: ``V dim`` (or ``F dim``) single precision floats of
  attribute data.

.. _sec-packed-texture:

Texture entries
***************

A texture entry holds a texture in the BC4, BC5, or BC7 block compression
formats, which encode each 4x4 block of texels in 8 or 16 bytes. The ``cuda``
and ``metal`` variants sample such textures directly using the hardware
texture units. Other variants decode the blocks when loading the scene. The
``mitsuba.pack_tex`` script (``python -m mitsuba.pack_tex <scene.xml>``) packs
all existing textures of a Mitsuba scene into a joint container and writes an
updated scene referencing it.

.. list-table::
    :widths: 10 15 15 15 15 30
    :header-rows: 1

    * - Format
      - Channels
      - Bytes per block
      - Bits per texel
      - Savings
      - Typical use
    * - BC4
      - 1
      - 8
      - 4
      - 2x
      - Roughness, opacity
    * - BC5
      - 2
      - 16
      - 8
      - 2x
      - Tangent-space normals (*x*, *y*), with *z* reconstructed on lookup
    * - BC7
      - 3 or 4
      - 16
      - 8
      - 4x
      - Color, optionally sRGB-encoded

An entry may include a complete MIP chain for the
``trilinear`` and ``anisotropic`` filters. Block-compressed textures are not
differentiable. The entry begins with the following header:

.. list-table::
    :widths: 20 80
    :header-rows: 1

    * - Type
      - Content
    * - :monosp:`char[4]`
      - The ASCII bytes ``BTEX``
    * - :monosp:`uint32`
      - Entry version, currently 1
    * - :monosp:`uint8`
      - The format number 4, 5, or 7
    * - :monosp:`uint8`
      - sRGB flag
    * - :monosp:`uint8`
      - The number of MIP levels
    * - :monosp:`uint8`
      - Channel count: 1 for BC4, 2 for BC5, and 3 or 4 for BC7
    * - :monosp:`uint32`
      - Width in texels
    * - :monosp:`uint32`
      - Height in texels

The header is followed by one compressed array per MIP level, starting with
the base level. A level of size :math:`w \times h` consists of the row-major
4x4 blocks covering :math:`\lceil w/4 \rceil \times \lceil h/4 \rceil`
texels. A chain of :math:`k` levels halves the resolution at every level
(rounding down, but never below one texel), and a complete chain ends at a
single texel. A reader that does not filter across MIP levels decompresses
only the base level. Textures with large uniform regions consist of many
identical blocks, which the LZ4 compression typically shrinks by half at a
small cost in loading time.
