[sgcl](../../README.md) › [encoding](../README.md) › [varint](README.md)

# sgcl::encoding::varint::read, async_read

```cpp
static expected<pair<uint64_t, size_t>, error> read(const slice<const byte>& at) noexcept;    // (1)
static expected<optional<uint64_t>, io::error> read(const io::buffered_reader& in);           // (2)
static async::task<expected<optional<uint64_t>, io::error>>
    async_read(const io::buffered_reader& in) noexcept;                                       // (3)
```

1. The varint at the front of `at`, and the bytes it took: Go's `binary.Uvarint`. Bytes that end before the
   number does are `unexpected_end` at their end, empty bytes included; a number past 64 bits — a tenth byte
   above 1 — is `out_of_range` at that byte. A longer encoding of a number than it needs (`80 00` for 0) is read,
   as Go reads it. The bytes after the number are not read.
2. The next varint of a stream, read a byte at a time from the [buffered_reader](../../io/buffered_reader/README.md): Go's
   `binary.ReadUvarint`. `nullopt` at the end of the stream before a number's first byte, so that a loop over a
   stream of numbers ends there; `io::errc::unexpected_eof` when the stream ends inside one; `out_of_range` of
   the `encoding` category past 64 bits; and the error of the stream when a read of it fails.
3. The same in a task, `co_await encoding::varint::async_read(in)`. The reader is a handle, its copies the same
   reader at one position: the task holds the one it is given for as long as it runs.

Ten bytes whose tenth has its high bit set are past 64 bits whatever follows, and refused at the tenth. Go's
`Uvarint` calls them cut short, and names the eleventh byte when there is one; its `ReadUvarint` refuses at the
tenth, as this does.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes the number is at the front of |
| `in` | the stream the number is read from |

## Return value

1. The number and the bytes it took, or the [error](../error/README.md) with its code and offset.
2. The number, `nullopt` at the end of the stream, or the `io::error`.
3. A task of the same.

## Complexity

Linear in the bytes of the number, ten at most.

## Exceptions

- (1) None.
- (2) What the read of the stream under `in` throws; the streams of the library throw nothing.
- (3) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> ids;
    for (uint64_t id : {7, 300, 70000}) {
        encoding::varint::append(ids, id);
    }
    auto [first, size] = encoding::varint::read(ids).value();
    println("{} in {} byte", first, size);

    io::buffered_reader in(io::buffer{ids});
    for (;;) {
        auto id = encoding::varint::read(in);
        if (!id || !*id) {
            break;  // an error (id.error()), or the end
        }
        println(**id);
    }

    vector<byte> cut = {byte(0x80), byte(0x80)};
    println(encoding::varint::read(cut).error().message());
}
```

Output:

```text
7 in 1 byte
7
300
70000
offset 2: the bytes end inside a varint
```

## See also

- [read_signed, async_read_signed](read_signed.md): a signed number
- [append](append.md), [write](write.md): the other way
- [sgcl::encoding::varint](README.md)
