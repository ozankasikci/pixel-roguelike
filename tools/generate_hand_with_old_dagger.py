#!/usr/bin/env python3

import argparse
import json
import math
import struct
from dataclasses import dataclass
from pathlib import Path

import numpy as np


JSON_CHUNK_TYPE = 0x4E4F534A
BIN_CHUNK_TYPE = 0x004E4942

COMPONENT_DTYPES = {
    5120: np.int8,
    5121: np.uint8,
    5122: np.int16,
    5123: np.uint16,
    5125: np.uint32,
    5126: np.float32,
}

TYPE_COMPONENTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}


@dataclass
class RawMeshData:
    positions: np.ndarray
    normals: np.ndarray
    indices: np.ndarray


def align4(value: int) -> int:
    return (value + 3) & ~3


def load_glb_raw(filepath: Path) -> RawMeshData:
    data = filepath.read_bytes()
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or length != len(data):
        raise ValueError(f"Invalid GLB header in {filepath}")

    offset = 12
    json_chunk = None
    bin_chunk = None
    while offset < len(data):
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk_data = data[offset:offset + chunk_length]
        offset += chunk_length
        if chunk_type == JSON_CHUNK_TYPE:
            json_chunk = json.loads(chunk_data.decode("utf-8"))
        elif chunk_type == BIN_CHUNK_TYPE:
            bin_chunk = chunk_data

    if json_chunk is None or bin_chunk is None:
        raise ValueError(f"Missing JSON or BIN chunk in {filepath}")

    primitive = json_chunk["meshes"][0]["primitives"][0]

    def read_accessor(accessor_index: int) -> np.ndarray:
        accessor = json_chunk["accessors"][accessor_index]
        buffer_view = json_chunk["bufferViews"][accessor["bufferView"]]
        dtype = np.dtype(COMPONENT_DTYPES[accessor["componentType"]])
        component_count = TYPE_COMPONENTS[accessor["type"]]
        count = accessor["count"]
        itemsize = dtype.itemsize
        stride = buffer_view.get("byteStride", itemsize * component_count)
        byte_offset = buffer_view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        array = np.ndarray(
            shape=(count, component_count),
            dtype=dtype,
            buffer=bin_chunk,
            offset=byte_offset,
            strides=(stride, itemsize),
        ).copy()
        if component_count == 1:
            return array.reshape(count)
        return array

    positions = read_accessor(primitive["attributes"]["POSITION"]).astype(np.float32)
    normals_accessor = primitive["attributes"].get("NORMAL")
    if normals_accessor is None:
        normals = np.tile(np.array([[0.0, 1.0, 0.0]], dtype=np.float32), (positions.shape[0], 1))
    else:
        normals = read_accessor(normals_accessor).astype(np.float32)
    indices = read_accessor(primitive["indices"]).astype(np.uint32)

    return RawMeshData(positions=positions, normals=normals, indices=indices)


def merge_meshes(parts: list[tuple[RawMeshData, np.ndarray]]) -> RawMeshData:
    positions = []
    normals = []
    indices = []
    base_index = 0

    for mesh, transform in parts:
        linear = transform[:3, :3].astype(np.float64, copy=False)
        translation = transform[:3, 3].astype(np.float64, copy=False)
        normal_matrix = np.linalg.inv(linear).T

        source_positions = mesh.positions.astype(np.float64, copy=False)
        source_normals = mesh.normals.astype(np.float64, copy=False)

        transformed_positions = np.empty_like(source_positions)
        transformed_normals = np.empty_like(source_normals)
        for axis in range(3):
            transformed_positions[:, axis] = (
                source_positions[:, 0] * linear[axis, 0]
                + source_positions[:, 1] * linear[axis, 1]
                + source_positions[:, 2] * linear[axis, 2]
                + translation[axis]
            )
            transformed_normals[:, axis] = (
                source_normals[:, 0] * normal_matrix[axis, 0]
                + source_normals[:, 1] * normal_matrix[axis, 1]
                + source_normals[:, 2] * normal_matrix[axis, 2]
            )
        lengths = np.linalg.norm(transformed_normals, axis=1, keepdims=True)
        transformed_normals = transformed_normals / np.maximum(lengths, 1e-8)

        positions.append(transformed_positions.astype(np.float32))
        normals.append(transformed_normals.astype(np.float32))
        indices.append(mesh.indices.astype(np.uint32) + base_index)
        base_index += len(mesh.positions)

    return RawMeshData(
        positions=np.vstack(positions),
        normals=np.vstack(normals),
        indices=np.concatenate(indices),
    )


def translation_matrix(x: float, y: float, z: float) -> np.ndarray:
    matrix = np.eye(4, dtype=np.float32)
    matrix[:3, 3] = [x, y, z]
    return matrix


def scale_matrix(x: float, y: float, z: float) -> np.ndarray:
    matrix = np.eye(4, dtype=np.float32)
    matrix[0, 0] = x
    matrix[1, 1] = y
    matrix[2, 2] = z
    return matrix


def rotation_x_matrix(degrees: float) -> np.ndarray:
    radians = math.radians(degrees)
    c = math.cos(radians)
    s = math.sin(radians)
    return np.array(
        [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, c, -s, 0.0],
            [0.0, s, c, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
        dtype=np.float32,
    )


def rotation_z_matrix(degrees: float) -> np.ndarray:
    radians = math.radians(degrees)
    c = math.cos(radians)
    s = math.sin(radians)
    return np.array(
        [
            [c, -s, 0.0, 0.0],
            [s, c, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ],
        dtype=np.float32,
    )


def generate_cube(size: float = 1.0) -> RawMeshData:
    h = size * 0.5
    positions = np.array(
        [
            [-h, -h, h], [h, -h, h], [h, h, h], [-h, h, h],
            [h, -h, -h], [-h, -h, -h], [-h, h, -h], [h, h, -h],
            [-h, -h, -h], [-h, -h, h], [-h, h, h], [-h, h, -h],
            [h, -h, h], [h, -h, -h], [h, h, -h], [h, h, h],
            [-h, h, h], [h, h, h], [h, h, -h], [-h, h, -h],
            [-h, -h, -h], [h, -h, -h], [h, -h, h], [-h, -h, h],
        ],
        dtype=np.float32,
    )
    normals = np.array(
        [
            [0, 0, 1], [0, 0, 1], [0, 0, 1], [0, 0, 1],
            [0, 0, -1], [0, 0, -1], [0, 0, -1], [0, 0, -1],
            [-1, 0, 0], [-1, 0, 0], [-1, 0, 0], [-1, 0, 0],
            [1, 0, 0], [1, 0, 0], [1, 0, 0], [1, 0, 0],
            [0, 1, 0], [0, 1, 0], [0, 1, 0], [0, 1, 0],
            [0, -1, 0], [0, -1, 0], [0, -1, 0], [0, -1, 0],
        ],
        dtype=np.float32,
    )
    indices = []
    for face in range(6):
        base = face * 4
        indices.extend([base + 0, base + 1, base + 2, base + 0, base + 2, base + 3])
    return RawMeshData(positions=positions, normals=normals, indices=np.array(indices, dtype=np.uint32))


def bend_fingers(hand: RawMeshData, bend_angle_deg: float, knuckle_frac: float = 0.75) -> RawMeshData:
    positions = hand.positions.copy()
    normals = hand.normals.copy()

    b_min = positions.min(axis=0)
    b_max = positions.max(axis=0)
    knuckle_y = b_min[1] + (b_max[1] - b_min[1]) * knuckle_frac
    finger_range = b_max[1] - knuckle_y
    if finger_range <= 1e-6:
        return RawMeshData(positions, normals, hand.indices.copy())

    center_z = (b_min[2] + b_max[2]) * 0.5
    max_angle = math.radians(bend_angle_deg)

    for i in range(len(positions)):
        if positions[i, 1] <= knuckle_y:
            continue
        frac = np.clip((positions[i, 1] - knuckle_y) / finger_range, 0.0, 1.0)
        angle = frac * max_angle
        c = math.cos(angle)
        s = math.sin(angle)

        rel_y = positions[i, 1] - knuckle_y
        rel_z = positions[i, 2] - center_z
        positions[i, 1] = knuckle_y + rel_y * c - rel_z * s
        positions[i, 2] = center_z + rel_y * s + rel_z * c

        normal_y = normals[i, 1]
        normal_z = normals[i, 2]
        normals[i, 1] = normal_y * c - normal_z * s
        normals[i, 2] = normal_y * s + normal_z * c

    return RawMeshData(positions=positions, normals=normals, indices=hand.indices.copy())


def generate_old_dagger() -> RawMeshData:
    unit_cube = generate_cube(1.0)
    parts: list[tuple[RawMeshData, np.ndarray]] = []

    def box(pos, size, rot_x=0.0, rot_z=0.0):
        transform = translation_matrix(*pos)
        if rot_z != 0.0:
            transform = transform @ rotation_z_matrix(rot_z)
        if rot_x != 0.0:
            transform = transform @ rotation_x_matrix(rot_x)
        transform = transform @ scale_matrix(*size)
        parts.append((unit_cube, transform))

    # Worn, compact dagger silhouette with slightly drooped guard arms.
    box((0.0, 0.105, 0.0), (0.030, 0.175, 0.010))
    box((0.0, 0.214, 0.0), (0.020, 0.045, 0.007))
    box((0.0, 0.018, 0.0), (0.040, 0.020, 0.016))
    box((-0.040, 0.010, 0.0), (0.038, 0.010, 0.014), rot_z=18.0)
    box((0.040, 0.010, 0.0), (0.038, 0.010, 0.014), rot_z=-18.0)
    box((0.0, -0.050, 0.0), (0.018, 0.085, 0.018))
    box((0.0, -0.105, 0.0), (0.026, 0.026, 0.026))

    return merge_meshes(parts)


def build_hand_with_old_dagger(reference_path: Path) -> RawMeshData:
    hand_raw = load_glb_raw(reference_path)
    gripping_hand = bend_fingers(hand_raw, bend_angle_deg=108.0)

    h_min = hand_raw.positions.min(axis=0)
    h_max = hand_raw.positions.max(axis=0)
    h_size = h_max - h_min
    knuckle_y = h_min[1] + h_size[1] * 0.75
    finger_range = h_max[1] - knuckle_y
    center_x = (h_min[0] + h_max[0]) * 0.5
    center_z = (h_min[2] + h_max[2]) * 0.5

    scale = h_size[1] / 0.40
    dagger_transform = translation_matrix(
        center_x - h_size[0] * 0.01,
        knuckle_y - h_size[1] * 0.01,
        center_z + finger_range * 0.18,
    )
    dagger_transform = dagger_transform @ rotation_z_matrix(-4.0)
    dagger_transform = dagger_transform @ scale_matrix(scale, scale, scale)

    return merge_meshes(
        [
            (gripping_hand, np.eye(4, dtype=np.float32)),
            (generate_old_dagger(), dagger_transform),
        ]
    )


def write_glb(mesh: RawMeshData, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)

    positions = np.asarray(mesh.positions, dtype=np.float32)
    normals = np.asarray(mesh.normals, dtype=np.float32)
    indices = np.asarray(mesh.indices, dtype=np.uint32)

    bin_blob = bytearray()
    buffer_views = []

    for payload, target in (
        (positions.tobytes(), 34962),
        (normals.tobytes(), 34962),
        (indices.tobytes(), 34963),
    ):
        while len(bin_blob) % 4 != 0:
            bin_blob.append(0)
        byte_offset = len(bin_blob)
        bin_blob.extend(payload)
        buffer_views.append(
            {
                "buffer": 0,
                "byteOffset": byte_offset,
                "byteLength": len(payload),
                "target": target,
            }
        )

    gltf = {
        "asset": {
            "version": "2.0",
            "generator": "generate_hand_with_old_dagger.py",
        },
        "buffers": [{"byteLength": len(bin_blob)}],
        "bufferViews": buffer_views,
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": int(len(positions)),
                "type": "VEC3",
                "min": positions.min(axis=0).tolist(),
                "max": positions.max(axis=0).tolist(),
            },
            {
                "bufferView": 1,
                "componentType": 5126,
                "count": int(len(normals)),
                "type": "VEC3",
            },
            {
                "bufferView": 2,
                "componentType": 5125,
                "count": int(len(indices)),
                "type": "SCALAR",
            },
        ],
        "meshes": [
            {
                "name": "hand_with_old_dagger",
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "NORMAL": 1},
                        "indices": 2,
                        "mode": 4,
                    }
                ],
            }
        ],
        "nodes": [{"mesh": 0, "name": "hand_with_old_dagger"}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }

    json_blob = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    while len(json_blob) % 4 != 0:
        json_blob += b" "
    while len(bin_blob) % 4 != 0:
        bin_blob.append(0)

    total_length = 12 + 8 + len(json_blob) + 8 + len(bin_blob)
    with output_path.open("wb") as handle:
        handle.write(struct.pack("<4sII", b"glTF", 2, total_length))
        handle.write(struct.pack("<II", len(json_blob), JSON_CHUNK_TYPE))
        handle.write(json_blob)
        handle.write(struct.pack("<II", len(bin_blob), BIN_CHUNK_TYPE))
        handle.write(bin_blob)


def save_preview(mesh: RawMeshData, preview_path: Path) -> None:
    try:
        import matplotlib.pyplot as plt
        from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    except ImportError as exc:
        raise RuntimeError("matplotlib is required for --preview") from exc

    triangles = mesh.positions[mesh.indices.reshape(-1, 3)]
    center = mesh.positions.mean(axis=0)
    radius = np.max(np.linalg.norm(mesh.positions - center, axis=1))

    fig = plt.figure(figsize=(12, 4), dpi=160)
    views = [
        ("Perspective", 20, -60),
        ("Front", 0, -90),
        ("Side", 0, 0),
    ]

    for subplot_index, (title, elev, azim) in enumerate(views, start=1):
        ax = fig.add_subplot(1, 3, subplot_index, projection="3d")
        collection = Poly3DCollection(
            triangles,
            facecolor="#b8b0a4",
            edgecolor="#3a332d",
            linewidths=0.15,
            alpha=1.0,
        )
        ax.add_collection3d(collection)
        ax.set_title(title, fontsize=9)
        ax.view_init(elev=elev, azim=azim)
        ax.set_xlim(center[0] - radius, center[0] + radius)
        ax.set_ylim(center[1] - radius, center[1] + radius)
        ax.set_zlim(center[2] - radius, center[2] + radius)
        ax.set_box_aspect((1, 1, 1))
        ax.set_axis_off()

    fig.tight_layout()
    preview_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(preview_path, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate a hand-with-old-dagger GLB from a reference hand GLB.")
    parser.add_argument(
        "--reference",
        type=Path,
        default=Path("assets/meshes/hand_low_poly.glb"),
        help="Reference hand GLB path.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("assets/meshes/hand_with_old_dagger.glb"),
        help="Output GLB path.",
    )
    parser.add_argument(
        "--preview",
        type=Path,
        help="Optional preview image path.",
    )
    args = parser.parse_args()

    mesh = build_hand_with_old_dagger(args.reference)
    write_glb(mesh, args.output)
    if args.preview:
        save_preview(mesh, args.preview)

    bbox_min = mesh.positions.min(axis=0)
    bbox_max = mesh.positions.max(axis=0)
    print(f"Wrote {args.output}")
    print(f"Vertices: {len(mesh.positions)}")
    print(f"Triangles: {len(mesh.indices) // 3}")
    print(f"BBox min: {bbox_min.tolist()}")
    print(f"BBox max: {bbox_max.tolist()}")
    if args.preview:
        print(f"Preview: {args.preview}")


if __name__ == "__main__":
    main()
