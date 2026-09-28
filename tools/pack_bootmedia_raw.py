"""Pack StopWatch boot animation in the raw partition layout used by firmware."""

import argparse
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("media", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--partition-size", type=lambda value: int(value, 0), required=True)
    args = parser.parse_args()

    manifest = args.manifest.read_bytes()
    media = args.media.read_bytes()
    if not manifest.startswith(b"canvas_width="):
        raise ValueError("invalid boot media manifest")
    if len(manifest) >= 0x1000:
        raise ValueError("boot media manifest exceeds 4 KiB")
    if 0x1000 + len(media) > args.partition_size:
        raise ValueError("boot media exceeds partition")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(manifest + bytes(0x1000 - len(manifest)) + media)


if __name__ == "__main__":
    main()
