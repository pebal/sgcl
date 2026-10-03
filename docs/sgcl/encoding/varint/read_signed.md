[sgcl](../../README.md) › [encoding](../README.md) › [varint](../varint.md)

# sgcl::encoding::varint::read_signed, async_read_signed

```cpp
static expected<pair<int64_t, size_t>, error> read_signed(const slice<const byte>& at)       // (1)
    noexcept;
static expected<optional<int64_t>, io::error> read_signed(const io::buffered_reader& in);    // (2)
static async::task<expected<optional<int64_t>, io::error>>
    async_read_signed(const io::buffered_reader& in) noexcept;                               // (3)
```

The signed forms of [read](read.md): the varint read as `read` reads it, and its zigzag undone — 0, 1, 2, 3 to 0,
-1, 1, -2. Go's `binary.Varint` and `binary.ReadVarint`.

1. The number at the front of `at`, and the bytes it took; the errors of `read`.
2. The next number of a stream, read a byte at a time from the [buffered_reader](../../io/buffered_reader.md): `nullopt`
   at the end of the stream before a number's first byte, and the errors of `read`.
3. The same in a task, `co_await encoding::varint::async_read_signed(in)`; the task holds the reader it is given
   for as long as it runs.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the bytes the number is at the front of |
| `in` | the stream the number is read from |

## Return value

1. The number and the bytes it took, or the [error](../error.md) with its code and offset.
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
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int64_t> sum(io::buffered_reader in) {
    int64_t total = 0;
    for (;;) {
        auto n = co_await encoding::varint::async_read_signed(in);
        if (!n || !*n) {
            co_return total;
        }
        total += **n;
    }
}

int main() {
    vector<byte> deltas;
    for (int64_t d : {100, -3, -40, 7}) {
        encoding::varint::append_signed(deltas, d);
    }
    auto [first, size] = encoding::varint::read_signed(deltas).value();
    println("{} in {} bytes", first, size);
    println("{}", async::run(sum(io::buffered_reader(io::buffer{deltas}))));
}
```

Output:

```text
100 in 2 bytes
64
```

## See also

- [read, async_read](read.md): an unsigned number
- [append_signed](append_signed.md), [write_signed](write_signed.md): the other way
- [sgcl::encoding::varint](../varint.md)
