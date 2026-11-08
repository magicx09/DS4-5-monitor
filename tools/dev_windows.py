"""Windows native development entry point; only Python's standard library is needed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
ROOT = Path(os.environ.get('DS45_DEV_HOME', str(REPO / '.local'))).resolve()
DEPS = ROOT / 'deps'
LOGS = ROOT / 'logs'
BUILD = ROOT / 'build'
LLVM = DEPS / 'llvm-mingw-20261104-ucrt-x86_64'
SDK_PIN = '079c6f39023649b154152db30f1d781e884879bc'
TUSB_PIN = '98741b55959564746998d0453c32d87064629d7c'


def output(args):
    return subprocess.check_output([str(a) for a in args], text=True, encoding='utf-8').strip()


def environment():
    env = os.environ.copy()
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    cmake = shutil.which('cmake')
    ninja = shutil.which('ninja')
    if vswhere.exists():
        vs = Path(output([vswhere, '-latest', '-products', '*', '-property', 'installationPath']))
        tools = vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake'
        if not cmake and (tools / 'CMake/bin/cmake.exe').exists():
            cmake = str(tools / 'CMake/bin/cmake.exe')
        if not ninja and (tools / 'Ninja/ninja.exe').exists():
            ninja = str(tools / 'Ninja/ninja.exe')
    git = shutil.which('git')
    bash = Path(git).resolve().parent.parent / 'bin/bash.exe' if git else Path('missing-bash')
    required = [DEPS / 'bin/arm-none-eabi-gcc.exe', LLVM / 'bin/clang++.exe', bash,
                DEPS / 'host-tools/picotool/picotool.exe', DEPS / 'host-tools/pioasm/pioasm.exe']
    missing = [str(p) for p in required if not p.exists()]
    if not cmake or not ninja or not git or missing:
        raise RuntimeError(f'Missing dependencies: {missing}; CMake={cmake}; Ninja={ninja}; Git={git}')
    env['PATH'] = os.pathsep.join([str(DEPS / 'bin'), str(LLVM / 'bin'),
                                  str(Path(cmake).parent), str(Path(ninja).parent),
                                  str(Path(sys.executable).parent), env.get('PATH', '')])
    env['PICO_SDK_PATH'] = (DEPS / 'pico-sdk').as_posix()
    env['PICO_TOOLCHAIN_PATH'] = DEPS.as_posix()
    env['PYTHON'] = Path(sys.executable).as_posix()
    env['PYTHONUTF8'] = '1'
    return env, cmake, ninja, str(bash)


def run(args, name, env):
    LOGS.mkdir(exist_ok=True)
    path = LOGS / f'{name}.log'
    print(f'[{name}]', flush=True)
    with path.open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(a) for a in args], cwd=REPO, env=env,
                                   stdout=log, stderr=subprocess.STDOUT)
        process.wait()
    text = path.read_text(encoding='utf-8', errors='replace')
    if process.returncode:
        print('\n'.join(text.splitlines()[-45:]))
        raise RuntimeError(f'{name} failed ({process.returncode}). See {path}')
    if 'tests' in name or 'verify' in name:
        for line in text.splitlines():
            if any(s in line for s in ('tests,', 'tests passed', 'check_', 'Total Test time')):
                print(line)
    print(f'  OK: {path.relative_to(ROOT)}', flush=True)


def doctor(env, cmake, ninja, bash):
    for path, expected in [(DEPS / 'pico-sdk', SDK_PIN), (DEPS / 'pico-sdk/lib/tinyusb', TUSB_PIN),
                           (REPO / 'lib/WDL', 'afc0f78dca4a1743a948839ac75f4d7059d739c3'),
                           (REPO / 'lib/opus', '2d862ea14b233e5a3f3afaf74d96050691af3cd5')]:
        actual = output(['git', '-C', path, 'rev-parse', 'HEAD'])
        if actual != expected:
            raise RuntimeError(f'Pin mismatch: {path}: {actual} != {expected}')
        print(f'{path.relative_to(ROOT)}: {actual}')
    for command in ([cmake, '--version'], [ninja, '--version'], [DEPS / 'bin/arm-none-eabi-gcc.exe', '--version'],
                    [LLVM / 'bin/clang++.exe', '--version'], [sys.executable, '--version'], [bash, '--version']):
        print(output(command).splitlines()[0])


def tests(env, cmake, ninja, frames=False):
    target = BUILD / 'host'
    run([cmake, '-S', REPO / 'tests/host', '-B', target, '-G', 'Ninja',
         f'-DCMAKE_MAKE_PROGRAM={ninja}', f'-DCMAKE_CXX_COMPILER={LLVM.as_posix()}/bin/clang++.exe',
         f'-DTINYUSB_SOURCE={DEPS.as_posix()}/pico-sdk/lib/tinyusb/src',
         '-DCMAKE_BUILD_TYPE=Debug'], 'host-configure', env)
    run([cmake, '--build', target, '-j', '8'], 'host-build', env)
    ctest = Path(cmake).parent / 'ctest.exe'
    run([ctest, '--test-dir', target, '--output-on-failure', '-V'], 'host-tests', env)
    if frames:
        for row in (14, 16):
            dest = ROOT / 'frames' / f'prop{row}'
            dest.mkdir(parents=True, exist_ok=True)
            frame_env = dict(env, OLED_ZH_DUMP=dest.as_posix())
            run([target / f'oled_zh_prop{row}_tests.exe'], f'frames-prop{row}', frame_env)


def ds4_firmware(row, env, cmake, ninja, bash):
    target = BUILD / f'ds4-prop{row}'
    run([cmake, '-S', REPO, '-B', target, '-G', 'Ninja', f'-DCMAKE_MAKE_PROGRAM={ninja}',
         '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
         '-DPICO_BOARD=pico2_w', '-DDS5_CLOCK_KHZ=150000', '-DENABLE_COMPANION_VENDOR=OFF',
         '-DENABLE_SERIAL=OFF', '-DENABLE_DS4=ON',
         '-DOLED_ZH=ON', f'-DOLED_SETTINGS_ROW_HEIGHT={row}', f'-DVERSION={(REPO / "VERSION").read_text().strip()}',
         f'-DPICO_SDK_PATH={DEPS.as_posix()}/pico-sdk', f'-DPICO_TOOLCHAIN_PATH={DEPS.as_posix()}',
         f'-DPython3_EXECUTABLE={Path(sys.executable).as_posix()}',
         f'-Dpicotool_DIR={DEPS.as_posix()}/host-tools/picotool',
         f'-Dpioasm_DIR={DEPS.as_posix()}/host-tools/pioasm'], f'ds4-prop{row}-configure', env)
    run([cmake, '--build', target, '--target', 'ds5-bridge', '-j', '8'], f'ds4-prop{row}-build', env)
    elf = target / 'ds5-bridge-oled.elf'
    uf2 = target / 'ds5-bridge-oled.uf2'
    run([sys.executable, REPO / 'tools/check_flash_reserved.py', elf, uf2], f'ds4-prop{row}-verify-flash', env)
    run([bash, (REPO / 'tools/check_ram_residency.sh').as_posix(), elf.as_posix()], f'ds4-prop{row}-verify-ram', env)
    run([bash, (REPO / 'tools/check_xip_setup.sh').as_posix(), elf.as_posix(), '2', '11'], f'ds4-prop{row}-verify-xip', env)
    dest = ROOT / 'firmware' / 'ds4-experimental'
    dest.mkdir(parents=True, exist_ok=True)
    for ext in ('uf2', 'elf'):
        shutil.copy2(target / f'ds5-bridge-oled.{ext}', dest / f'DS4-5-monitor-{(REPO / "VERSION").read_text().strip()}-pico2w-zh-prop{row}.{ext}')
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dest.iterdir()) if p.suffix in ('.elf', '.uf2')}
    (dest / 'SHA256SUMS.txt').write_text(''.join(f'{v}  {k}\n' for k, v in hashes.items()), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', nargs='?', default='all', choices=['doctor', 'test', 'build', 'all', 'font', 'frames', 'ds4'])
    # This checkout's active OLED layout is prop16.  Keep one firmware
    # variant in the normal workflow so the tested artifact cannot be
    # accidentally replaced by the retired prop14 layout.
    parser.add_argument('--row', choices=['16'], default='16')
    args = parser.parse_args()
    env, cmake, ninja, bash = environment()
    doctor(env, cmake, ninja, bash)
    rows = (16,)
    if args.action in ('test', 'all', 'frames', 'ds4'):
        tests(env, cmake, ninja, args.action == 'frames')
    if args.action in ('ds4', 'build', 'all'):
        for row in rows:
            ds4_firmware(row, env, cmake, ninja, bash)
    if args.action == 'font':
        run([sys.executable, REPO / 'tools/build_fusion_12px_prop_font.py', REPO / 'fonts/fusion-pixel-12px-proportional'], 'font', env)
    print('Done.', flush=True)


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(1)
