import struct
import os


def size_fmt(num, suffix='B'):
    for unit in ['', 'Ki', 'Mi', 'Gi', 'Ti', 'Pi']:
        if abs(num) < 1024.0:
            return "%3.1f %s%s" % (num, unit, suffix)
        num /= 1024.0
    return "%.1f %s%s" % (num, 'Yi', suffix)


def read(filename):
    import numpy as np

    with open(filename, 'rb') as f:
        def unpack(fmt):
            result = struct.unpack(fmt, f.read(struct.calcsize(fmt)))
            return result if len(result) > 1 else result[0]

        if f.read(12) != 'tensor_file\0'.encode('utf8'):
            raise Exception('Invalid tensor file (header not recognized)')

        if unpack('<BB') != (1, 0):
            raise Exception('Invalid tensor file (unrecognized '
                            'file format version)')

        field_count = unpack('<I')
        size = os.stat(filename).st_size
        print('Loading tensor data from \"%s\" .. (%s, %i field%s)'
            % (filename, size_fmt(size),
               field_count, 's' if field_count > 1 else ''))

        # Maps from Struct.EType field in Mitsuba
        dtype_map = {
            1: np.uint8,
            2: np.int8,
            3: np.uint16,
            4: np.int16,
            5: np.uint32,
            6: np.int32,
            7: np.uint64,
            8: np.int64,
            9: np.float16,
            10: np.float32,
            11: np.float64
        }

        fields = {}
        for i in range(field_count):
            field_name = f.read(unpack('<H')).decode('utf8')
            field_ndim = unpack('<H')
            field_dtype = dtype_map[unpack('<B')]
            field_offset = unpack('<Q')
            field_shape = unpack('<' + 'Q' * field_ndim)
            fields[field_name] = (field_offset, field_dtype, field_shape)

        result = {}
        for k, v in fields.items():
            f.seek(v[0])
            result[k] = np.fromfile(f, dtype=v[1],
                                    count=np.prod(v[2])).reshape(v[2])
    return result


def write(filename, align=8, **kwargs):
    import numpy as np
    assert isinstance(align, int), "Alignment must be an integer, make sure to unpack the tensor dictionary using **"

    with open(filename, 'wb') as f:
        # Identifier
        f.write('tensor_file\0'.encode('utf8'))

        # Version number
        f.write(struct.pack('<BB', 1, 0))

        # Number of fields
        f.write(struct.pack('<I', len(kwargs)))

        # Maps to Struct.EType field in Mitsuba
        dtype_map = {
            np.uint8: 1,
            np.int8: 2,
            np.uint16: 3,
            np.int16: 4,
            np.uint32: 5,
            np.int32: 6,
            np.uint64: 7,
            np.int64: 8,
            np.float16: 9,
            np.float32: 10,
            np.float64: 11
        }

        offsets = {}
        fields = dict(kwargs)

        # Write all fields
        for k, v in fields.items():
            if type(v) is str:
                v = np.frombuffer(v.encode('utf8'), dtype=np.uint8)
            else:
                v = np.ascontiguousarray(v)
            fields[k] = v

            # Field identifier
            label = k.encode('utf8')
            f.write(struct.pack('<H', len(label)))
            f.write(label)

            # Field dimension
            f.write(struct.pack('<H', v.ndim))

            found = False
            for dt in dtype_map.keys():
                if dt == v.dtype:
                    found = True
                    f.write(struct.pack('B', dtype_map[dt]))
                    break
            if not found:
                raise Exception("Unsupported dtype: %s" % str(v.dtype))

            # Field offset (unknown for now)
            offsets[k] = f.tell()
            f.write(struct.pack('<Q', 0))

            # Field sizes
            f.write(struct.pack('<' + ('Q' * v.ndim), *v.shape))

        for k, v in fields.items():
            # Set field offset
            pos = f.tell()

            # Pad to requested alignment
            pos = (pos + align - 1) // align * align

            f.seek(offsets[k])
            f.write(struct.pack('<Q', pos))
            f.seek(pos)

            # Field data
            v.tofile(f)

        print('Wrote \"%s\" (%s)' % (filename, size_fmt(f.tell())))
