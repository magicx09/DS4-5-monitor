"""Install pinned build dependencies into .local (or DS45_DEV_HOME).

Requires Python 3, Git for Windows, CMake and Ninja (PATH or Visual Studio).
No administrator access, registry changes, flashing or global PATH changes.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import urllib.request
import zipfile

REPO = Path(__file__).resolve().parents[1]
DEV_HOME = Path(os.environ.get('DS45_DEV_HOME', str(REPO / '.local'))).resolve()
DEPS = DEV_HOME / 'deps'
LOCK = json.loads((REPO / 'tools/dependencies-windows.json').read_text(encoding='utf-8'))

def run(*args, env=None):
    subprocess.run([str(a) for a in args], check=True, env=env)

def head(path):
    return subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()

def checkout(path, url, ref):
    if path.exists():
        if head(path) != ref:
            raise RuntimeError(f'{path} is not pinned at {ref}; existing checkout left untouched')
        return
    path.mkdir(parents=True)
    run('git', '-C', path, 'init', '-q')
    run('git', '-C', path, 'remote', 'add', 'origin', url)
    run('git', '-C', path, 'fetch', '--depth=1', 'origin', ref)
    run('git', '-C', path, 'checkout', '--detach', 'FETCH_HEAD')
    if head(path) != ref:
        raise RuntimeError(f'Unexpected commit in {path}')

def archive(spec, dest, sentinel):
    if sentinel.exists():
        return
    cache = DEV_HOME / 'downloads'
    cache.mkdir(parents=True, exist_ok=True)
    name = spec['url'].rsplit('/', 1)[-1]
    path = cache / name
    checksum = spec.get('sha256', spec.get('download_sha256'))
    if not path.exists():
        print('Downloading', name, flush=True)
        with urllib.request.urlopen(spec['url'], timeout=120) as source, path.open('wb') as target:
            shutil.copyfileobj(source, target)
    if hashlib.file_digest(path.open('rb'), 'sha256').hexdigest() != checksum:
        raise RuntimeError(f'Checksum mismatch: {path}; remove the bad download and retry')
    stage = cache / (name + '.unpacked')
    stage.mkdir(exist_ok=True)
    with zipfile.ZipFile(path) as z:
        z.extractall(stage)
    children = list(stage.iterdir())
    source = children[0] if len(children) == 1 and children[0].is_dir() else stage
    shutil.copytree(source, dest, dirs_exist_ok=True)

def main():
    if os.name != 'nt':
        raise RuntimeError('Windows only; Linux instructions are in docs/BUILD.md')
    cmake, ninja = shutil.which('cmake'), shutil.which('ninja')
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    if vswhere.exists() and (not cmake or not ninja):
        found = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-property', 'installationPath'], text=True).strip()
        base = Path(found) / 'Common7/IDE/CommonExtensions/Microsoft/CMake'
        if (base / 'CMake/bin/cmake.exe').exists(): cmake = cmake or str(base / 'CMake/bin/cmake.exe')
        if (base / 'Ninja/ninja.exe').exists(): ninja = ninja or str(base / 'Ninja/ninja.exe')
    if not cmake or not ninja or not shutil.which('git'):
        raise RuntimeError('Install Git for Windows, CMake and Ninja first (or Visual Studio CMake tools)')
    DEPS.mkdir(parents=True, exist_ok=True)
    archive(LOCK['arm_toolchain'], DEPS, DEPS / 'bin/arm-none-eabi-gcc.exe')
    llvm = DEPS / 'llvm-mingw-20261104-ucrt-x86_64'
    archive(LOCK['host_compiler'], llvm, llvm / 'bin/clang++.exe')
    sdk = DEPS / 'pico-sdk'
    pins = LOCK['git_dependencies']
    checkout(sdk, 'https://github.com/raspberrypi/pico-sdk.git', pins['deps/pico-sdk'])
    run('git', '-C', sdk, 'submodule', 'update', '--init', '--recursive')
    tusb = sdk / 'lib/tinyusb'
    if head(tusb) != pins['deps/pico-sdk/lib/tinyusb']:
        dirty = subprocess.check_output(['git', '-C', str(tusb), 'status', '--porcelain'], text=True)
        if dirty.strip(): raise RuntimeError('TinyUSB has local changes; left untouched')
        run('git', '-C', tusb, 'fetch', '--depth=1', 'origin', pins['deps/pico-sdk/lib/tinyusb'])
        run('git', '-C', tusb, 'checkout', '--detach', pins['deps/pico-sdk/lib/tinyusb'])
    run('git', '-C', REPO, 'submodule', 'update', '--init', '--recursive')
    picotool = DEV_HOME / 'sources/picotool'
    checkout(picotool, 'https://github.com/raspberrypi/picotool.git', LOCK['picotool']['commit'])
    env = dict(os.environ)
    env['PATH'] = os.pathsep.join([str(llvm / 'bin'), str(Path(ninja).parent), env.get('PATH', '')])
    env['PICO_SDK_PATH'] = sdk.as_posix()
    for name, source, flags in [
        ('pioasm', sdk / 'tools/pioasm', ['-DPIOASM_FLAT_INSTALL=1', '-DPIOASM_VERSION_STRING=2.3.1']),
        ('picotool', picotool, ['-DPICOTOOL_FLAT_INSTALL=1', '-DPICOTOOL_NO_LIBUSB=1']),
    ]:
        build = DEV_HOME / ('build-tools/' + name)
        prefix = DEPS / ('host-tools/' + name)
        run(cmake, '-S', source, '-B', build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_MAKE_PROGRAM=' + str(ninja),
            '-DCMAKE_C_COMPILER=' + (llvm / 'bin/clang.exe').as_posix(),
            '-DCMAKE_CXX_COMPILER=' + (llvm / 'bin/clang++.exe').as_posix(),
            '-DCMAKE_INSTALL_PREFIX=' + prefix.as_posix(), *flags, env=env)
        run(cmake, '--build', build, '-j', '8', env=env)
        run(cmake, '--install', build, env=env)
    print('Ready: python tools/dev_windows.py all')

if __name__ == '__main__':
    main()
