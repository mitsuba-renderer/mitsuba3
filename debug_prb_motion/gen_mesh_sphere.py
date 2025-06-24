# Generate high-quality unit sphere (radius 1) as OBJ
import numpy as np



# Subdivide icosahedron to create a high-quality sphere mesh
def create_icosphere(level=3):
    # Create icosahedron
    t = (1.0 + np.sqrt(5.0)) / 2.0
    verts = np.array([
        [-1,  t,  0], [ 1,  t,  0], [-1, -t,  0], [ 1, -t,  0],
        [ 0, -1,  t], [ 0,  1,  t], [ 0, -1, -t], [ 0,  1, -t],
        [ t,  0, -1], [ t,  0,  1], [-t,  0, -1], [-t,  0,  1],
    ], dtype=np.float64)
    verts /= np.linalg.norm(verts, axis=1)[:, None]
    faces = [
        [0,11,5], [0,5,1], [0,1,7], [0,7,10], [0,10,11],
        [1,5,9], [5,11,4], [11,10,2], [10,7,6], [7,1,8],
        [3,9,4], [3,4,2], [3,2,6], [3,6,8], [3,8,9],
        [4,9,5], [2,4,11], [6,2,10], [8,6,7], [9,8,1],
    ]

    def midpoint(a, b, cache):
        nonlocal verts
        key = tuple(sorted((a, b)))
        if key in cache:
            return cache[key]
        v = (verts[a] + verts[b]) / 2
        v /= np.linalg.norm(v)
        idx = len(verts)
        verts = np.vstack([verts, v])
        cache[key] = idx
        return idx

    for _ in range(level):
        new_faces = []
        cache = {}
        for tri in faces:
            a, b, c = tri
            ab = midpoint(a, b, cache)
            bc = midpoint(b, c, cache)
            ca = midpoint(c, a, cache)
            new_faces += [
                [a, ab, ca],
                [b, bc, ab],
                [c, ca, bc],
                [ab, bc, ca],
            ]
        faces = new_faces
    return verts, faces

def write_obj(filename, verts, faces):
    with open(filename, 'w') as f:
        for v in verts:
            f.write(f"v {v[0]:.8f} {v[1]:.8f} {v[2]:.8f}\n")
        for face in faces:
            # OBJ is 1-indexed
            f.write(f"f {face[0]+1} {face[1]+1} {face[2]+1}\n")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="Generate a high-quality unit sphere OBJ file.")
    parser.add_argument("-o", "--output", type=str, default="sphere.obj", help="Output OBJ filename")
    parser.add_argument("-l", "--level", type=int, default=3, help="Subdivision level (3-6 recommended)")
    args = parser.parse_args()
    verts, faces = create_icosphere(args.level)
    write_obj(args.output, verts, faces)
    print(f"OBJ file written to {args.output} with {len(verts)} vertices and {len(faces)} faces.")
