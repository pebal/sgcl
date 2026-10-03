[sgcl](../../README.md) › [io](../README.md) › [buffer](../buffer.md)

# sgcl::io::buffer::seek

```cpp
expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin) const noexcept;
```

Moves the write position, as a file's seek does: to `offset` from the first byte held, from the position, or from
the end, by [seek_from](../seek_from.md). Reads are not moved: they take from the front whatever the position.
The position counts from the first byte held, [data](data.md), so a read moves it back with the bytes it takes.

A position past the end is allowed: the next write fills the gap with zeros, as Go's `os.File` and `pwrite` do,
where a `std::stringstream` refuses the seek. `seek(0, seek_from::end)` puts the position back at the end, where
writes append. A position before the first byte is refused, and the position stays where it was.

## Parameters

| Parameter | Description |
|---|---|
| `offset` | the distance from where `from` says, negative backwards |
| `from` | `seek_from::begin` (the first byte held, the default), `seek_from::current` (the position) or `seek_from::end` (the end) |

## Return value

The new position, from the first byte held; or an error, operation `seek`, path `buffer`, the position left where
it was: `std::errc::invalid_argument` when the position would be before the first byte, `std::errc::value_too_large`
when `offset` added to where `from` counts from passes what an `int64_t` holds.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out("header: ?\nbody\n");
    out.seek(8);
    out.write("1");
    out.seek(0, io::seek_from::end);
    out.write("more\n");
    print("{}", out.text());

    println("{}", *out.seek(-5, io::seek_from::current));
    println("{}", out.seek(-1).error().message());
}
```

Output:

```text
header: 1
body
more
15
seek buffer: Invalid argument
```

## See also

- [tell](../mixin/seeker/tell.md), [rewind](../mixin/seeker/rewind.md): the position, the position at the start
- [write, async_write](write.md)
- [sgcl::io::buffer](../buffer.md)
