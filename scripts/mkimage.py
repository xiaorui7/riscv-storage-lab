"""Create a deterministic, small KTFS fixture from its documented disk structs.

Supports a flat root and direct + single-indirect file blocks, sufficient for
the demo executable. This is a fixture builder, not a general filesystem tool.
"""
import argparse
from pathlib import Path
import struct

BLOCK = 512
BLOCKS = 16384
BITMAP_BLOCKS = 4
INODE_BLOCKS = 6
DATA_START = 1 + BITMAP_BLOCKS + INODE_BLOCKS


def create_image(files):
    image = bytearray(BLOCKS * BLOCK)
    struct.pack_into("<IIIH", image, 0, BLOCKS, BITMAP_BLOCKS, INODE_BLOCKS, 0)
    next_block = DATA_START

    def mark(block):
        image[BLOCK + block // 8] |= 1 << (block % 8)

    for block in range(DATA_START):
        mark(block)

    def allocate(data):
        nonlocal next_block
        block = next_block
        next_block += 1
        if next_block > BLOCKS or len(data) > BLOCK:
            raise ValueError("Fixture exceeds image limits")
        mark(block)
        image[block * BLOCK:block * BLOCK + len(data)] = data
        return block - DATA_START

    def inode(number, data):
        chunks = [data[i:i + BLOCK] for i in range(0, len(data), BLOCK)]
        if len(chunks) > 131:
            raise ValueError("Fixture file exceeds direct + single-indirect limit")
        blocks = [allocate(chunk) for chunk in chunks]
        direct = (blocks[:3] + [0, 0, 0])[:3]
        indirect = 0
        if len(blocks) > 3:
            indirect = allocate(struct.pack("<" + "I" * len(blocks[3:]), *blocks[3:]))
        offset = (1 + BITMAP_BLOCKS) * BLOCK + number * 32
        struct.pack_into("<8I", image, offset, len(data), 0, *direct, indirect, 0, 0)

    entries = bytearray()
    names = set()
    if len(files) > 95:
        raise ValueError("At most 95 fixture files")
    for number, path in enumerate(files, 1):
        name = path.name.encode("ascii")
        if not 1 <= len(name) <= 13 or name in names:
            raise ValueError("Use unique ASCII filenames of 1–13 bytes")
        names.add(name)
        inode(number, path.read_bytes())
        entries.extend(struct.pack("<H14s", number, name))
    inode(0, entries)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--file", action="append", type=Path, default=[])
    args = parser.parse_args()
    data = create_image(args.file)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(data)
    print(f"Created {args.output}: {len(data)} bytes, {len(args.file)} files")


if __name__ == "__main__":
    main()
