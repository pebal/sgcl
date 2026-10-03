[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::varint

```cpp
#include "sgcl/encoding/binary.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class varint;
}
```

`sgcl::encoding::varint` is the variable-length integer of Go and Protocol Buffers, LEB128 without a sign: seven
bits a byte, the low ones first, the high bit set on every byte but the last — a small number is one byte, the
largest ten. A signed number is zigzagged first, so that a small negative number is short too. It is Go's
`binary.AppendUvarint`, `PutUvarint`, `Uvarint` and `ReadUvarint` and their signed forms; `std` has none.

Every member is static. A number goes at the back of a vector ([append](append.md)) or into the front of
bytes that are there ([write](write.md)), and is read from the front of bytes or from a
[buffered_reader](../../io/buffered_reader/README.md) ([read](read.md)), a stream of numbers read to its end.

## Rules

- A signed varint is zigzagged first — 0, -1, 1, -2 to 0, 1, 2, 3 — so that a small negative number is short too
  (Go's `AppendVarint`): the `_signed` forms.
- A read of bytes answers the number and the bytes it took. Bytes that end before the number does are
  `unexpected_end` at their end; a number past 64 bits — a tenth byte above 1 — is `out_of_range` at that byte. A
  longer encoding of a number than it needs (`80 00` for 0) is read, as Go reads it.
- A read of a [buffered_reader](../../io/buffered_reader/README.md) answers `nullopt` at the end of the stream before a number's
  first byte — a loop over a stream of numbers ends there — `io::errc::unexpected_eof` when the stream ends inside
  one, and `out_of_range` of the `encoding` category past 64 bits; `co_await async_read(in)` is the same in a
  task. The reader is a handle: its copies are the same reader, at one position, and the task holds the one it is
  given for as long as it runs.
- Nothing allocates but `append`, which grows the vector, and nothing waits but the reads of a stream.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `binary.AppendUvarint`, `PutUvarint`, `Uvarint`, `ReadUvarint` | `varint::append`, `write`, `read(bytes)`, `read(const io::buffered_reader&)` |
| `binary.AppendVarint`, `PutVarint`, `Varint`, `ReadVarint` | `varint::append_signed`, `write_signed`, `read_signed` |
| `binary.MaxVarintLen64` | `varint::max_size` |
| `Uvarint`'s `n == 0` (too few bytes) and `n < 0` (past 64 bits) | an [error](../error/README.md): `unexpected_end` at the end, `out_of_range` at the byte |
| `ReadUvarint`'s `io.EOF`, `io.ErrUnexpectedEOF` and its overflow error | `nullopt`, `io::errc::unexpected_eof`, `out_of_range` of the `encoding` category |
| ten bytes whose tenth has its high bit set: `Uvarint` calls them cut short, and names the eleventh byte when there is one | past 64 bits whatever follows, refused at the tenth, as Go's `ReadUvarint` refuses them |

The tests hold both sides to that last difference by name.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |

## Member objects

| Constant | Description |
|---|---|
| `max_size` | `static constexpr size_t`, 10: the most bytes a number of 64 bits takes |

## Member functions

#### Writing

| Function | Description |
|---|---|
| [append](append.md) | a number at the back of a vector (static) |
| [append_signed](append_signed.md) | a signed number at the back of a vector (static) |
| [write](write.md) | a number into the front of the bytes (static) |
| [write_signed](write_signed.md) | a signed number into the front of the bytes (static) |

#### Reading

| Function | Description |
|---|---|
| [read, async_read](read.md) | the number at the front of the bytes, or the next number of a stream (static) |
| [read_signed, async_read_signed](read_signed.md) | the same, signed (static) |

## Complexity

Linear in the bytes of the number, ten at most.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> out;
    encoding::varint::append(out, 300);
    encoding::varint::append_signed(out, -3);
    encoding::little_endian::append_u16(out, 0xABCD);
    println(encoding::hex::encode(out));
    auto [value, size] = encoding::varint::read(out).value();
    auto [signed_value, more] = encoding::varint::read_signed(out.as_slice(size)).value();
    println("{} {} {}", value, signed_value, size + more);
}
```

Output:

```text
ac0205cdab
300 -3 3
```

## See also

- [big_endian](../big_endian/README.md), [little_endian](../little_endian/README.md): numbers of a fixed size
- [buffered_reader](../../io/buffered_reader/README.md): a stream read a byte at a time
- [error](../error/README.md), [errc](../errc.md): why bytes are not a number
- [sgcl::encoding](../README.md)
