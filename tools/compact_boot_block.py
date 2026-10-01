#!/usr/bin/env python3
"""Losslessly compact a v1/v2 RGB565 boot stream into same-color delta runs.

Requires firmware with delta_runs_rgb565_black_v3 support. Usage:
  python tools/compact_boot_block.py boot_block.txt boot_block.bin output_dir
"""
import argparse
from pathlib import Path
import struct

FORMAT = 'delta_runs_rgb565_black_v3'


def varint(value):
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return result


def compact(manifest_text, data):
    fields = dict(line.split('=', 1) for line in manifest_text.splitlines() if '=' in line)
    source_format = fields['stream_format']
    if source_format not in ('delta_varint_rgb565_black_v1', 'delta_varint_rgb565_black_v2'):
        raise ValueError('expected a v1/v2 pixel stream')
    cells = int(fields['grid_width']) * int(fields['grid_height'])
    if not 0 < cells <= 512 * 512 or len(data) != int(fields['binary_size']):
        raise ValueError('invalid grid or binary size')
    position = 0
    result = bytearray()

    def read(count):
        nonlocal position
        if position + count > len(data):
            raise ValueError('truncated stream')
        value = data[position:position + count]
        position += count
        return value

    def read_varint():
        value = 0
        for shift in (0, 7, 14, 21):
            byte = read(1)[0]
            value |= (byte & 127) << shift
            if not byte & 128:
                return value
        raise ValueError('overlong varint')

    for _ in range(int(fields['frame_count'])):
        count = int.from_bytes(read(4 if source_format.endswith('v2') else 2), 'little')
        if count > cells:
            raise ValueError('too many changes')
        previous = -1
        runs = []
        for _ in range(count):
            value = read_varint()
            delta = value >> 1
            index = delta if previous < 0 else previous + delta
            if index >= cells or (previous >= 0 and not delta):
                raise ValueError('invalid cell index')
            previous = index
            color = 0 if value & 1 else int.from_bytes(read(2), 'little')
            if runs and index == runs[-1][0] + runs[-1][1] and color == runs[-1][2]:
                runs[-1][1] += 1
            else:
                runs.append([index, 1, color])
        result.extend(struct.pack('<I', len(runs)))
        previous_end = -1
        for start, length, color in runs:
            explicit_delta = start != previous_end + 1
            delta = start if previous_end < 0 else start - previous_end
            result.extend(varint((length << 2) | (explicit_delta << 1) | (color == 0)))
            if explicit_delta:
                result.extend(varint(delta))
            if color:
                result.extend(struct.pack('<H', color))
            previous_end = start + length - 1
    if position != len(data):
        raise ValueError('trailing stream bytes')
    fields.update(stream_format=FORMAT, binary_size=str(len(result)))
    # A generator's optional digest must describe the newly compacted stream.
    if 'binary_sha256' in fields:
        import hashlib
        fields['binary_sha256'] = hashlib.sha256(result).hexdigest()
    return '\n'.join(f'{key}={value}' for key, value in fields.items()) + '\n', bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('binary', type=Path)
    parser.add_argument('output_dir', type=Path)
    args = parser.parse_args()
    text, binary = compact(args.manifest.read_text(), args.binary.read_bytes())
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / 'boot_block.txt').write_text(text, encoding='utf-8')
    (args.output_dir / 'boot_block.bin').write_bytes(binary)
    print(f'{len(binary)} bytes (from {args.binary.stat().st_size}), {FORMAT}')


if __name__ == '__main__':
    main()
