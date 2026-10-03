[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::big_endian

```cpp
#include "sgcl/encoding/binary.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class big_endian;
}
```

`sgcl::encoding::big_endian` holds the numbers of 16, 32 and 64 bits of a binary format that writes the high byte
first: network order, PNG, Java's streams. It is Go's `binary.BigEndian`, and C++23's `std::byteswap` with C++20's
`std::endian` without the question of the order the processor uses: `read_` takes the number from the front of
the bytes, `write_` puts it there, `append_` adds it at the back of a vector, whatever the processor.
[little_endian](../little_endian/README.md) is the same for the low byte first; [varint](../varint/README.md) is a number in as few
bytes as it needs.

Every member is static: `encoding::big_endian::read_u32(header)`.

## Rules

- The bytes of a read or a write hold at least the number's size: a precondition, checked by `assert` as
  `operator[]` is. A read at an offset is a read of the slice from there, `v.as_slice(4)`.
- Each is a loop of bytes and shifts that the compiler turns into one load or store and, for the order the
  processor does not use, one byte swap.
- Nothing allocates but `append_`, which grows the vector; nothing throws, nothing waits.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `binary.BigEndian.Uint16`, `Uint32`, `Uint64` | `big_endian::read_u16`, `read_u32`, `read_u64` |
| `binary.BigEndian.PutUint16`, `PutUint32`, `PutUint64` | `big_endian::write_u16`, `write_u32`, `write_u64` |
| `binary.BigEndian.AppendUint16`, `AppendUint32`, `AppendUint64` | `big_endian::append_u16`, `append_u32`, `append_u64` |
| a slice too short: a panic | a precondition, checked by `assert` |
| `binary.Read`, `binary.Write` of a structure | none: a structure is read and written a number at a time |

## Member functions

#### Reading

| Function | Description |
|---|---|
| [read_u16](read_u16.md) | the number of 16 bits at the front of the bytes (static) |
| [read_u32](read_u32.md) | the number of 32 bits at the front of the bytes (static) |
| [read_u64](read_u64.md) | the number of 64 bits at the front of the bytes (static) |

#### Writing

| Function | Description |
|---|---|
| [write_u16](write_u16.md) | a number of 16 bits into the front of the bytes (static) |
| [write_u32](write_u32.md) | a number of 32 bits into the front of the bytes (static) |
| [write_u64](write_u64.md) | a number of 64 bits into the front of the bytes (static) |
| [append_u16](append_u16.md) | a number of 16 bits at the back of a vector (static) |
| [append_u32](append_u32.md) | a number of 32 bits at the back of a vector (static) |
| [append_u64](append_u64.md) | a number of 64 bits at the back of a vector (static) |

## Complexity

Constant.

## Example

A header of eight bytes, a magic number and a size, in network order:

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 8> header = {};
    encoding::big_endian::write_u32(header, 0xCAFEBABE);
    encoding::big_endian::write_u32(header.as_slice(4), 1234);
    println(encoding::hex::encode(header));
    println("{:x} {}", encoding::big_endian::read_u32(header),
            encoding::big_endian::read_u32(header.as_slice(4)));
}
```

Output:

```text
cafebabe000004d2
cafebabe 1234
```

## See also

- [little_endian](../little_endian/README.md): the low byte first
- [varint](../varint/README.md): a number in as few bytes as it needs
- [hex](../hex/README.md): to look at the bytes
- [sgcl::encoding](../README.md)
