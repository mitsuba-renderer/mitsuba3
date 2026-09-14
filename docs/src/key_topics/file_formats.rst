.. _sec-file-formats:

File formats
============

Besides standard image and mesh formats, Mitsuba reads a few file formats of
its own. All of them use little endian byte order.

- The :ref:`scene XML format <sec-file-format>` describes which plugins to
  instantiate and how to connect them.

- The :ref:`serialized mesh format <shape-serialized>` stores indexed
  triangle meshes in a compact form that loads quickly.

- The :ref:`packed texture container <sec-pack-format>` described below
  stores block-compressed textures in the memory layout of GPU texture units.

.. _sec-pack-format:

Packed texture container (``.packed``)
--------------------------------------

A ``.packed`` file holds a set of textures in the BC4, BC5, and BC7 *block
compression* formats, which encode each 4x4 block of texels in 8 or 16 bytes.
This significantly reduces GPU memory usage compared to unpacked representations.
The ``cuda`` and ``metal`` variants sample such textures directly using the
hardware texture units. Other variants decode the blocks when loading the
scene. The :ref:`bitmap <texture-bitmap>` texture plugin loads individual
entries of a container.

.. figtable::
    :label: table-bc-formats

    .. list-table::
        :widths: 10 12 30 12 12 12 12
        :header-rows: 1

        * - Format
          - Channels
          - Use case
          - Bytes per block
          - Bits per texel
          - Uncompressed
          - Savings
        * - BC4
          - 1
          - Roughness, opacity, and other grayscale maps
          - 8
          - 4
          - 8
          - 2x
        * - BC5
          - 2
          - Normal maps (X,Y components)
          - 16
          - 8
          - 32
          - 4x
        * - BC7
          - 3 or 4
          - LDR albedo and other color data
          - 16
          - 8
          - 32
          - 4x

An entry may include a complete MIP chain for the ``trilinear`` and
``anisotropic`` filters.

The file consists of a header, the data of all entries, a table of fixed-size
records followed by the entry names, and a footer that locates the table. The
file format is designed to be written in a streaming fashion, while allowing
random access to contents.

.. figtable::
    :label: table-pack-format

    .. list-table::
        :widths: 20 80
        :header-rows: 1

        * - Type
          - Content
        * - :monosp:`char[12]`
          - The ASCII bytes ``MIPACK.TEX`` followed by two zero bytes
        * - :monosp:`uint32`
          - File version, currently 1
        * - :math:`\rightarrow`
          - The data of every entry, at the offsets given in the records
        * - :monosp:`record`
          - Start of the table: :math:`n` records of 44 bytes each, holding
            four :monosp:`uint8` fields (the format number 4, 5, or 7, an
            sRGB flag, the number of MIP levels, and a reserved byte), four
            :monosp:`uint32` fields (the width, height, channel count, and
            the length of the name in bytes), and three :monosp:`uint64`
            fields (the file offset of the entry's data, its size in the
            file, and its uncompressed size)
        * - :monosp:`string`
          - The :math:`n` names as UTF-8 bytes, in the order of the records
            and without separators
        * - :monosp:`uint64`
          - Footer: the file offset of the table
        * - :monosp:`uint32`
          - The number of entries :math:`n`

The uncompressed data of an entry consists of the row-major 4x4 blocks of each
MIP level, starting with the base level, where a level of size :math:`w \times
h` covers :math:`\lceil w/4 \rceil \times \lceil h/4 \rceil` blocks. Each
level halves the resolution of the previous one (rounding down, but never
below one texel), and a complete chain ends at a single texel.
