#!/usr/bin/env python3
"""Prepare the same pinned StopWatch libraries as the PowerShell helper."""
import argparse
from pathlib import Path
import subprocess

REPO = Path(__file__).resolve().parents[1]
DEPENDENCIES = (
    ('M5GFX', 'https://github.com/m5stack/M5GFX.git', '0.2.19', '53a7184'),
    ('M5PM1', 'https://github.com/m5stack/M5PM1.git', '1.0.6', '8f1f1a6'),
    ('M5IOE1', 'https://github.com/m5stack/M5IOE1.git', '1.0.8', '37db048'),
    ('BMI270_SensorAPI', 'https://github.com/boschsensortec/BMI270_SensorAPI.git', None,
     '41129fcfe39c583ee5462d79195741945d51c1fe'),
)


def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout.strip()


def prepare(destination):
    destination.mkdir(parents=True, exist_ok=True)
    for name, url, tag, commit in DEPENDENCIES:
        path = destination / name
        if not path.exists():
            if tag:
                run('git', 'clone', '--depth', '1', '--branch', tag, url, str(path))
            else:
                run('git', 'clone', '--depth', '1', url, str(path))
                run('git', '-C', str(path), 'fetch', '--depth', '1', 'origin', commit)
                run('git', '-C', str(path), 'checkout', '--detach', commit)
        actual = run('git', '-c', f'safe.directory={path}', '-C', str(path), 'rev-parse', 'HEAD')
        if not actual.startswith(commit):
            raise RuntimeError(f'{name} is at {actual}; expected {commit}. Inspect it before building.')

    for name in ('M5PM1', 'M5IOE1'):
        path = destination / name / 'CMakeLists.txt'
        source = path.read_text(encoding='utf-8')
        if '"M5GFX"' not in source:
            source = source.replace('"espressif__i2c_bus"', '"espressif__i2c_bus"\n        "M5GFX"')
            if '"M5GFX"' not in source:
                raise RuntimeError(f'Could not patch {path}')
            path.write_text(source, encoding='utf-8')

    path = destination / 'M5GFX/src/lgfx/v1/panel/Panel_AMOLED.cpp'
    source = path.read_text(encoding='utf-8')
    if 'Send the PSRAM row through the SPI' not in source:
        old = '''                auto lb = buf[i & 1];//s->getDMABuffer(wb);
                memcpy(lb,  &_frame_buffer[fbpos], wb);
                fbpos += stride; // next line
                bus->writeBytes(lb, wb, false, true);'''
        new = '''                auto lb = buf[i & 1];//s->getDMABuffer(wb);
                if (lb)
                {
                    memcpy(lb, &_frame_buffer[fbpos], wb);
                }
                fbpos += stride; // next line
                // The SPI flip buffers can fail to resize when BLE fragments
                // internal DMA memory. Send the PSRAM row through the SPI
                // register path instead of dereferencing a null buffer.
                bus->writeBytes(lb ? lb : &_frame_buffer[fbpos - stride], wb, false, lb != nullptr);'''
        if old not in source:
            raise RuntimeError(f'Unexpected AMOLED transfer code in {path}')
        source = source.replace(old, new)
        path.write_text(source, encoding='utf-8')
    if 'Reserve full-width DMA rows' not in source:
        old = '''            buf[0] = bus->getDMABuffer(wb);
            buf[1] = bus->getDMABuffer(wb);'''
        new = '''            // Reserve full-width DMA rows even for narrow dirty rectangles.
            // Flip buffers must not shrink/grow as BLE fragments DMA memory.
            buf[0] = bus->getDMABuffer(stride);
            buf[1] = bus->getDMABuffer(stride);'''
        if old not in source:
            raise RuntimeError(f'Unexpected AMOLED DMA reservation in {path}')
        path.write_text(source.replace(old, new), encoding='utf-8')
    print(f'StopWatch dependencies ready: {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=REPO.parent)
    prepare(parser.parse_args().directory.resolve())
