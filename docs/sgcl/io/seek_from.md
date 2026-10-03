[sgcl](../README.md) › [io](README.md)

# sgcl::io::seek_from

```cpp
#include "sgcl/io/req.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class seek_from { begin, current, end };
}
```

Where the offset of a seek counts from: the first byte, the position, or the end of the stream. It is the second
argument of every `seek` of io ([file::seek](file/seek.md), [buffer::seek](buffer/seek.md)) and of the
requirement [req::seeker](req/seeker.md): Go's `io.SeekStart`, `io.SeekCurrent` and `io.SeekEnd`, POSIX's
`SEEK_SET`, `SEEK_CUR` and `SEEK_END`, as an enumeration. The result of a seek counts from the first byte, whichever
the enumerator.

| Value | Description |
|---|---|
| `begin` | the offset counts from the first byte of the stream |
| `current` | the offset counts from the position; `seek(0, seek_from::current)` is the position, as `tell()` |
| `end` | the offset counts from the end of the stream; `seek(0, seek_from::end)` is the size |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("digits.txt", "0123456789");
    io::file digits = *io::open("digits.txt");

    println("{}", *digits.seek(-3, io::seek_from::end));
    println("{}", *digits.read_all_text());

    digits.seek(2, io::seek_from::begin);
    println("{}", *digits.seek(3, io::seek_from::current));
    println("{}", *digits.read_all_text());
}
```

Output:

```text
7
789
5
56789
```

## See also

- [req::seeker](req/seeker.md): a stream with a position
- [mixin::seeker](mixin/seeker.md): `tell`, `size`, `rewind` over a seek
- [file::seek](file/seek.md), [buffer::seek](buffer/seek.md)
