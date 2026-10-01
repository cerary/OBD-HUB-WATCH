#!/usr/bin/env python3
"""Check the real boot player + LVGL, including malformed/legacy run cases.

Run on Linux/WSL after generating .cache/ui-preview with the UI preview tool:
  python3 tools/test_boot_runs.py /path/to/bootmedia_package
The package needs boot_block.txt and boot_block.bin. Output RGB565 frames and
the executable are stored in .cache/boot-runs-test.
"""
import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    cache = repo / '.cache/ui-preview'
    out = repo / '.cache/boot-runs-test'
    out.mkdir(parents=True, exist_ok=True)
    objects = sorted(p for p in cache.glob('*.o')
                     if p.name.split('_')[0].isdigit() and int(p.name.split('_')[0]) >= 1000)
    if len(objects) <= 100:
        raise SystemExit('Generate the LVGL UI preview cache first')
    subprocess.run(['gcc', '-std=c11', '-O2', '-ffunction-sections', '-fdata-sections',
                    '-DLV_CONF_KCONFIG_EXTERNAL_INCLUDE="sdkconfig.h"', '-DLV_CONF_SKIP=1',
                    '-I' + str(repo / 'tools/host_test/boot_runs_support'),
                    '-I' + str(repo / 'tools/host_test'), '-I' + str(cache),
                    '-I' + str(repo / 'main'), '-I' + str(repo / 'managed_components/lvgl__lvgl'),
                    '-c', str(repo / 'tools/host_test/test_boot_runs.c'),
                    '-o', str(out / 'test.o')], check=True)
    subprocess.run(['gcc', '-Wl,--gc-sections', str(out / 'test.o'), *map(str, objects),
                    '-lm', '-o', str(out / 'test')], check=True)
    subprocess.run([str(out / 'test'), str(args.package.resolve()), str(out / 'decoded.rgb565le')], check=True)


if __name__ == '__main__':
    main()
