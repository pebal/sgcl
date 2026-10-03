[sgcl](../../README.md) › [io](../README.md) › [error](../error.md)

# sgcl::io::error::count

```cpp
size_t count() const noexcept;
```

Returns what the operation had done when it failed: the bytes [read_full](../read_full.md) read before the stream
ended part way, `errc::unexpected_eof`, which are at the front of its buffer (the `n` Go's `io.ReadFull` returns
beside `io.ErrUnexpectedEOF`). The other errors of the module carry 0. An error the program makes carries the count
given to its [constructor](error.md), 0 by default.

## Parameters

None.

## Return value

The number of bytes done; 0 when the operation did none or does not count them. A count past 32 bits is held as
4294967295.

## Complexity

Constant.

## Exceptions

None.

## Notes

The count lives in the bytes an `error_code` leaves free between its value and its category: an error is four words,
as it was without it, and an `expected<T, io::error>`, the result of every read and write, is no larger for it.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer tail("last 13 bytes");
    vector<byte> record(64);
    auto r = io::read_full(tail, record);
    println("{}: {}", r.error().message(), r.error().count());
    println("{}", string(record.as_slice().first(r.error().count())));
    println("{}", io::open("missing.txt").error().count());
}
```

Output:

```text
read: unexpected end of stream: 13
last 13 bytes
0
```

## See also

- [read_full](../read_full.md): fills a buffer, or says how much it got
- [is_eof](is_eof.md): whether the stream ended part way
- [sgcl::io::error](../error.md)
