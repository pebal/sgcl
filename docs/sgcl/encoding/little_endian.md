[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::little_endian

```cpp
#include "sgcl/encoding/binary.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class little_endian;
}
```

`sgcl::encoding::little_endian` holds the numbers of 16, 32 and 64 bits of a binary format that writes the low
byte first: the order of the processors of today, of ZIP and of most file formats of the PC. It is Go's
`binary.LittleEndian`, and the same as [big_endian](big_endian.md) in the other order: `read_` takes the number
from the front of the bytes, `write_` puts it there, `append_` adds it at the back of a vector, whatever the
processor.

Every member is static: `encoding::little_endian::read_u32(entry.as_slice(18))`.

## Rules

- The bytes of a read or a write hold at least the number's size: a precondition, checked by `assert` as
  `operator[]` is. A read at an offset is a read of the slice from there, `v.as_slice(4)`.
- Each is a loop of bytes and shifts that the compiler turns into one load or store and, for the order the
  processor does not use, one byte swap.
- Nothing allocates but `append_`, which grows the vector; nothing throws, nothing waits.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `binary.LittleEndian.Uint16`, `Uint32`, `Uint64` | `little_endian::read_u16`, `read_u32`, `read_u64` |
| `binary.LittleEndian.PutUint16`, `PutUint32`, `PutUint64` | `little_endian::write_u16`, `write_u32`, `write_u64` |
| `binary.LittleEndian.AppendUint16`, `AppendUint32`, `AppendUint64` | `little_endian::append_u16`, `append_u32`, `append_u64` |
| a slice too short: a panic | a precondition, checked by `assert` |
| `binary.Read`, `binary.Write` of a structure | none: a structure is read and written a number at a time |

## Member functions

#### Reading

| Function | Description |
|---|---|
| [read_u16](little_endian/read_u16.md) | the number of 16 bits at the front of the bytes (static) |
| [read_u32](little_endian/read_u32.md) | the number of 32 bits at the front of the bytes (static) |
| [read_u64](little_endian/read_u64.md) | the number of 64 bits at the front of the bytes (static) |

#### Writing

| Function | Description |
|---|---|
| [write_u16](little_endian/write_u16.md) | a number of 16 bits into the front of the bytes (static) |
| [write_u32](little_endian/write_u32.md) | a number of 32 bits into the front of the bytes (static) |
| [write_u64](little_endian/write_u64.md) | a number of 64 bits into the front of the bytes (static) |
| [append_u16](little_endian/append_u16.md) | a number of 16 bits at the back of a vector (static) |
| [append_u32](little_endian/append_u32.md) | a number of 32 bits at the back of a vector (static) |
| [append_u64](little_endian/append_u64.md) | a number of 64 bits at the back of a vector (static) |

## Complexity

Constant.

## Example

The start of a ZIP file's local header: its signature, the version and the flags:

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> header;
    encoding::little_endian::append_u32(header, 0x04034B50);
    encoding::little_endian::append_u16(header, 20);
    encoding::little_endian::append_u16(header, 0);
    println(encoding::hex::encode(header));
    println("{:x} {}", encoding::little_endian::read_u32(header),
            encoding::little_endian::read_u16(header.as_slice(4)));
}
```

Output:

```text
504b030414000000
4034b50 20
```

## See also

- [big_endian](big_endian.md): the high byte first
- [varint](varint.md): a number in as few bytes as it needs
- [hex](hex.md): to look at the bytes
- [sgcl::encoding](README.md)
