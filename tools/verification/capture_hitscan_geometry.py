"""Capture hitbox fixtures using the verified TF2 client geometry routine.

Windows/x64 only. Map a caller-provided DLL copy with DONT_RESOLVE_DLL_REFERENCES;
DllMain and imported game interfaces are never initialized. The pinned function
and its direct callees are pure geometry, verified through headless IDA MCP.
No game process or installed binary is modified. This does not test networking,
weapon spread, damage, or live hit registration.
"""
import argparse
import ctypes as ctypes
import hashlib
import json
import math
from pathlib import Path
import struct

CLIENT_SHA256 = "b6f45c74805edacf0cf61d965f658158fc22960f66d583ef3a6549eb56696b95"
INTERSECT_HITBOX_RVA = 0x1BE390


def transform(vector, matrix, point=True):
    return [sum(matrix[row][col] * vector[col] for col in range(3))
            + (matrix[row][3] if point else 0.0) for row in range(3)]


def capture(dll_path):
    digest = hashlib.sha256(dll_path.read_bytes()).hexdigest()
    if digest != CLIENT_SHA256:
        raise ValueError("Unverified client.dll build: refusing to execute a hardcoded address")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.LoadLibraryExW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_uint32]
    kernel.LoadLibraryExW.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = [ctypes.c_void_p]
    kernel.FreeLibrary.restype = ctypes.c_int
    image = kernel.LoadLibraryExW(str(dll_path.resolve()), None, 1)
    if not image:
        raise ctypes.WinError(ctypes.get_last_error())
    intersect = ctypes.CFUNCTYPE(ctypes.c_int32, *([ctypes.c_void_p] * 4))(
        image + INTERSECT_HITBOX_RVA)
    cosine, sine = math.cos(math.pi / 6), math.sin(math.pi / 6)
    rotations = [
        [[1, 0, 0], [0, 1, 0], [0, 0, 1]],
        [[cosine, -sine, 0], [sine, cosine, 0], [0, 0, 1]],
        [[.36, -.8, .48], [.48, .6, .64], [-.8, 0, .6]],
    ]
    mins, maxs = [-4, 2, -1], [2, 6, 3]
    center = [(lo + hi) * .5 for lo, hi in zip(mins, maxs)]
    fixtures = []
    try:
        for rotation_index, rotation in enumerate(rotations):
            for model_scale in [.65, 1., 2.]:
                # Native ClipRayToHitboxes unscales both ray and bone matrix
                # around the render origin before its orthonormal core test.
                bone = [[*(value * model_scale for value in row), offset]
                        for row, offset in zip(rotation, [80., -30., 16.])]
                unscaled_bone = (ctypes.c_float * 12)(
                    *(value for row in rotation for value in [*row, 0.]))
                for inset in [.5, 1.]:
                    half = [(hi - lo) * .5 * inset for lo, hi in zip(mins, maxs)]
                    inset_mins = [mid - extent for mid, extent in zip(center, half)]
                    inset_maxs = [mid + extent for mid, extent in zip(center, half)]
                    for x in [-3., -1., 5.]:
                        for y in [2.5, 4., 9.]:
                            origin = transform([x, y, -20.], bone)
                            direction = transform([0, 0, 1], bone, point=False)
                            for max_distance in [8. * model_scale, 40. * model_scale]:
                                local_start = transform([x, y, -20.],
                                    [[*row, 0.] for row in rotation])
                                delta = [row[2] * max_distance / model_scale for row in rotation]
                                ray = ctypes.create_string_buffer(80)
                                struct.pack_into("<3f", ray, 0, *local_start)
                                struct.pack_into("<3f", ray, 16, *delta)
                                ray[64] = 1
                                ray[65] = 1
                                hitbox = ctypes.create_string_buffer(68)
                                struct.pack_into("<6f", hitbox, 8, *inset_mins, *inset_maxs)
                                trace = ctypes.create_string_buffer(128)
                                struct.pack_into("<f", trace, 44, 1.)
                                side = intersect(ray, hitbox, unscaled_bone, trace)
                                fraction = struct.unpack_from("<f", trace, 44)[0]
                                fixtures.append({
                                    "name": f"rotation{rotation_index}/model{model_scale}/inset{inset}/x{x}/y{y}/range{max_distance}",
                                    "origin": origin, "direction": direction, "mins": mins, "maxs": maxs,
                                    "bone": bone, "inset": inset, "max_distance": max_distance,
                                    "native_hit": side >= 0 and fraction < 1.,
                                    "native_side": side, "native_fraction": fraction,
                                })
    finally:
        kernel.FreeLibrary(image)
    return {"client_sha256": digest, "intersect_hitbox_rva": hex(INTERSECT_HITBOX_RVA),
            "steam_build": "26334388", "fixture_count": len(fixtures),
            "scope": "Native pure geometry; interior/exterior/range fixtures avoid grazing tolerance boundaries.",
            "fixtures": fixtures}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-copy", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    results = capture(args.client_copy)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    metadata = {key: value for key, value in results.items() if key != "fixtures"}
    fixture_lines = ",\n".join("    " + json.dumps(fixture, separators=(",", ":"))
                                for fixture in results["fixtures"])
    serialized = json.dumps(metadata, indent=2)[:-2] + ",\n  \"fixtures\": [\n" + fixture_lines + "\n  ]\n}\n"
    args.output.write_text(serialized, encoding="utf-8")
    print(f"Captured {results['fixture_count']} native DLL fixtures; "
          f"{sum(f['native_hit'] for f in results['fixtures'])} hits")


if __name__ == "__main__":
    main()
