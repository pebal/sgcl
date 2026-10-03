[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](README.md)

# sgcl::io::buffered_reader::set_max_line

```cpp
void set_max_line(size_t n) const noexcept;
```

Sets the longest line [read_line](read_line.md), [read_until](read_until.md) and [lines](lines.md) accept, in bytes:
every byte before the `"\n"` or the delimiter is counted, a `"\r"` before the `"\n"` too (though `read_line` drops
it), and the `"\n"` or the delimiter is not; 0 for no bound, which is the default. A longer line is the error
`errc::line_too_long`, found as soon as the bytes buffered pass the bound, before the line is assembled whole, and
skipped whole: the next call reads the line after it. It is the bound of Go's `Scanner.Buffer`. A file is trusted
and needs none; a reader over a socket sets one, so that a peer cannot make it hold a line of any length.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the bound, in bytes; 0 for none |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

A line too long whose end has not been read yet is read on to its end before the error is returned, its bytes
dropped as they come, so the memory held stays within the bound; the call waits for that end as a read does, or for
the end of the stream. A read of the stream that fails on the way is the error returned instead.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("ok\nthis line is too long\nok again\n"));
    in.set_max_line(8);
    for (int i : range(3)) {
        auto line = in.read_line();
        println("{}", line ? string(**line) : line.error().message());
    }
}
```

Output:

```text
ok
read_line: line too long
ok again
```

## See also

- [max_line](max_line.md): the bound
- [read_line](read_line.md), [lines](lines.md): what it bounds
- [sgcl::io::buffered_reader](README.md)
