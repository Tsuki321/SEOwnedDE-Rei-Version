"""Capture isolated native VPhysics trajectories and check prediction reset determinism.

Requires Windows x64, Python, and a locally installed TF2; no compilation or game launch.
Run from any directory with --game pointing to the TF2 installation.
"""

import argparse
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct

arguments = argparse.ArgumentParser(description=__doc__)
arguments.add_argument('--game', type=Path, default=Path('C:/Program Files (x86)/Steam/steamapps/common/Team Fortress 2'))
arguments.add_argument('--output', type=Path)
options = arguments.parse_args()
GAME = options.game
SDK = (Path(__file__).resolve().parents[2] / 'SEOwnedDE/SEOwnedDE/src/SDK/TF2/vphysics.h').read_text(encoding='utf-8')
DLL_DIRECTORY = os.add_dll_directory(str(GAME / 'bin/x64'))
LIB = C.WinDLL(str(GAME / 'bin/x64/vphysics.dll'))
LIB.CreateInterface.argtypes = [C.c_char_p, C.POINTER(C.c_int)]
LIB.CreateInterface.restype = C.c_void_p

def methods(name):
    body = SDK.split('class ' + name + '\n{', 1)[1].split('\n};', 1)[0]
    return re.findall(r'virtual\s+[^;{]*?([~\w]+)\s*\([^;{]*', body)

NAMES = {name: methods(name) for name in ['IPhysicsEnvironment', 'IPhysicsObject', 'IPhysicsCollision']}

def call(obj, interface, name, result, *args):
    index = NAMES[interface].index(name)
    vtable = C.cast(C.cast(obj, C.POINTER(C.c_void_p))[0], C.POINTER(C.c_void_p))
    signature = C.WINFUNCTYPE(result, C.c_void_p, *[kind for kind, value in args])
    return signature(vtable[index])(obj, *[value for kind, value in args])

def raw_call(obj, index, result, *args):
    vtable = C.cast(C.cast(obj, C.POINTER(C.c_void_p))[0], C.POINTER(C.c_void_p))
    return C.WINFUNCTYPE(result, C.c_void_p, *[kind for kind, value in args])(vtable[index])(
        obj, *[value for kind, value in args])

class Vector(C.Structure):
    _fields_ = [('x', C.c_float), ('y', C.c_float), ('z', C.c_float)]
    def array(self): return [self.x, self.y, self.z]

class Params(C.Structure):
    _fields_ = [('center', C.c_void_p), ('mass', C.c_float), ('inertia', C.c_float),
                ('damping', C.c_float), ('rotdamping', C.c_float), ('rotlimit', C.c_float),
                ('name', C.c_char_p), ('game', C.c_void_p), ('volume', C.c_float),
                ('drag', C.c_float), ('collisions', C.c_bool)]

class Solid(C.Structure):
    _fields_ = [('index', C.c_int), ('name', C.c_char * 512), ('parent', C.c_char * 512),
                ('surface', C.c_char * 512), ('center', Vector), ('params', Params)]

class VCollide(C.Structure):
    _fields_ = [('flags', C.c_ushort), ('descsize', C.c_ushort),
                ('solids', C.POINTER(C.c_void_p)), ('keys', C.c_char_p)]

assert C.sizeof(Params) == 64 and C.sizeof(Solid) == 1616 and Solid.params.offset == 1552

def defaults(): return Params(None, 1, 1, .1, .1, .05, b'CALIBRATION', None, 0, 1, False)

PARSE_KEY = C.WINFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_char_p, C.c_char_p)
SET_DEFAULTS = C.WINFUNCTYPE(None, C.c_void_p, C.c_void_p)
@PARSE_KEY
def parse_key(this, data, key, value): pass
@SET_DEFAULTS
def set_defaults(this, data): C.cast(data, C.POINTER(Solid)).contents.params = defaults()
HANDLER_VTABLE = (C.c_void_p * 2)(C.cast(parse_key, C.c_void_p), C.cast(set_defaults, C.c_void_p))
HANDLER = C.c_void_p(C.addressof(HANDLER_VTABLE))

def read_vpk(target):
    directory = GAME / 'tf/tf2_misc_dir.vpk'
    data = directory.read_bytes()
    magic, version, treesize = struct.unpack_from('<III', data)
    assert magic == 0x55aa1234 and version in (1, 2)
    header = 28 if version == 2 else 12
    offset = header
    def string():
        nonlocal offset
        end = data.index(b'\0', offset)
        value = data[offset:end].decode()
        offset = end + 1
        return value
    while True:
        extension = string()
        if not extension: break
        while True:
            path = string()
            if not path: break
            while True:
                name = string()
                if not name: break
                crc, preload, archive, entryoffset, length, terminator = struct.unpack_from('<IHHIIH', data, offset)
                assert terminator == 65535
                offset += 18
                prefix = data[offset:offset + preload]
                offset += preload
                filename = ('' if path == ' ' else path + '/') + name + '.' + extension
                if filename != target: continue
                if archive == 32767:
                    return prefix + data[header + treesize + entryoffset:header + treesize + entryoffset + length]
                with directory.with_name(f'tf2_misc_{archive:03}.vpk').open('rb') as handle:
                    handle.seek(entryoffset)
                    return prefix + handle.read(length)
    raise FileNotFoundError(target)

physics = LIB.CreateInterface(b'VPhysics031', None)
collision = LIB.CreateInterface(b'VPhysicsCollision007', None)
surfaces = LIB.CreateInterface(b'VPhysicsSurfaceProps001', None)
if not physics or not collision or not surfaces:
    raise RuntimeError('The installed DLL does not provide the supported VPhysics interfaces')
raw_call(surfaces, 1, C.c_int, (C.c_char_p, b'calibration'),
         (C.c_char_p, b'default { density 2000 friction 0.8 elasticity 0.2 }'))

def environment():
    env = raw_call(physics, 5, C.c_void_p)
    call(env, 'IPhysicsEnvironment', 'SetGravity', None, (C.c_void_p, C.byref(Vector(0, 0, -800))))
    call(env, 'IPhysicsEnvironment', 'SetAirDensity', None, (C.c_float, 2))
    call(env, 'IPhysicsEnvironment', 'SetSimulationTimestep', None, (C.c_float, 1 / 66))
    return env

def destroy_environment(env): raw_call(physics, 6, None, (C.c_void_p, env))

def sample(env, shape, params, count=60):
    # Match the production reset order, including repeated environment setters.
    call(env, 'IPhysicsEnvironment', 'SetGravity', None, (C.c_void_p, C.byref(Vector(0, 0, -800))))
    call(env, 'IPhysicsEnvironment', 'SetAirDensity', None, (C.c_float, 2))
    call(env, 'IPhysicsEnvironment', 'SetSimulationTimestep', None, (C.c_float, 1 / 66))
    call(env, 'IPhysicsEnvironment', 'Simulate', None, (C.c_float, 1 / 66))
    call(env, 'IPhysicsEnvironment', 'ResetSimulationClock', None)
    angle = Vector(0, 0, 0)
    body = call(env, 'IPhysicsEnvironment', 'CreatePolyObject', C.c_void_p,
                (C.c_void_p, shape), (C.c_int, 0), (C.c_void_p, C.byref(Vector(0, 0, 100))),
                (C.c_void_p, C.byref(angle)), (C.c_void_p, C.byref(params)))
    assert body
    drag = C.c_float(params.drag)
    call(body, 'IPhysicsObject', 'SetDragCoefficient', None, (C.c_void_p, C.byref(drag)), (C.c_void_p, C.byref(drag)))
    call(body, 'IPhysicsObject', 'SetVelocityInstantaneous', None,
         (C.c_void_p, C.byref(Vector(1200, 0, 200))), (C.c_void_p, C.byref(Vector(600, 0, 0))))
    call(body, 'IPhysicsObject', 'Wake', None)
    axis_drag = []
    for axis in range(3):
        direction = Vector(*[1 if i == axis else 0 for i in range(3)])
        axis_drag.append(call(body, 'IPhysicsObject', 'CalculateLinearDrag', C.c_float, (C.c_void_p, C.byref(direction))))
    positions = []
    for tick in range(count):
        call(env, 'IPhysicsEnvironment', 'Simulate', None, (C.c_float, 1 / 66))
        pos = Vector()
        call(body, 'IPhysicsObject', 'GetPosition', None, (C.c_void_p, C.byref(pos)), (C.c_void_p, None))
        positions.append(pos.array())
    mass = call(body, 'IPhysicsObject', 'GetMass', C.c_float)
    call(env, 'IPhysicsEnvironment', 'DestroyObject', None, (C.c_void_p, body))
    return positions, mass, axis_drag

results = {'vphysics_sha256': hashlib.sha256((GAME / 'bin/x64/vphysics.dll').read_bytes()).hexdigest(),
           'conditions': {'tick_interval': 1 / 66, 'gravity': 800, 'density': 2,
                          'origin': [0, 0, 100], 'velocity': [1200, 0, 200], 'spin': [600, 0, 0],
                          'empty_environment_primed': True, 'shared_environment_across_models': True}, 'models': {}}
shared_environment = environment()
for model in ['models/weapons/w_models/w_grenade_grenadelauncher.phy',
              'models/weapons/w_models/w_stickybomb.phy', 'models/weapons/w_models/w_cannonball.phy']:
    data = read_vpk(model)
    size, identity, solid_count, checksum = struct.unpack_from('<IIII', data)
    collide = VCollide()
    buffer = C.create_string_buffer(data[size:])
    call(collision, 'IPhysicsCollision', 'VCollideLoad', None,
         (C.c_void_p, C.byref(collide)), (C.c_int, solid_count), (C.c_void_p, buffer),
         (C.c_int, len(data) - size), (C.c_bool, False))
    assert collide.flags & 32767
    keyvalues = collide.keys
    parser = call(collision, 'IPhysicsCollision', 'VPhysicsKeyParserCreate', C.c_void_p,
                  (C.c_char_p, keyvalues))
    solid = Solid()
    while not raw_call(parser, 2, C.c_bool):
        name = raw_call(parser, 1, C.c_char_p)
        if name == b'solid':
            raw_call(parser, 3, None, (C.c_void_p, C.byref(solid)), (C.c_void_p, C.byref(HANDLER)))
            break
        raw_call(parser, 9, None)
    call(collision, 'IPhysicsCollision', 'VPhysicsKeyParserDestroy', None, (C.c_void_p, parser))
    assert solid.params.mass > 0
    solid.params.name = b'CALIBRATION'
    solid.params.game = None
    solid.params.collisions = False
    center = Vector(solid.center.x, solid.center.y, solid.center.z)
    if solid.params.center: solid.params.center = C.addressof(center)
    env = shared_environment
    first, mass, drag = sample(env, collide.solids[solid.index], solid.params)
    repeated, _, _ = sample(env, collide.solids[solid.index], solid.params)
    fresh = environment()
    reference, _, _ = sample(fresh, collide.solids[solid.index], solid.params)
    destroy_environment(fresh)
    maximum = max(abs(a - b) for left, right in zip(first, repeated) for a, b in zip(left, right))
    fresh_maximum = max(abs(a - b) for left, right in zip(first, reference) for a, b in zip(left, right))
    results['models'][model] = {'mass': mass, 'inertia_scale': solid.params.inertia,
                              'damping': solid.params.damping, 'rotational_damping': solid.params.rotdamping,
                              'axis_drag': drag, 'repeated_trial_max_error': maximum,
                              'fresh_environment_max_error': fresh_maximum, 'positions': first}
    call(collision, 'IPhysicsCollision', 'VCollideUnload', None, (C.c_void_p, C.byref(collide)))
    print(model, 'mass', mass, 'repeat error', maximum, 'fresh error', fresh_maximum, flush=True)
destroy_environment(shared_environment)
if options.output:
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
else:
    print(json.dumps(results, indent=2))
assert all(model['repeated_trial_max_error'] <= 1e-5 and model['fresh_environment_max_error'] <= 1e-5 for model in results['models'].values()), 'Native reset equivalence failed'
print('Native reset check complete')
