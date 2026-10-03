[sgcl](../../README.md) › [io](../README.md) › [req](README.md)

# sgcl::io::req::seeker

```cpp
#include "sgcl/io/req.h"   // or "sgcl/io.h"

namespace sgcl::io::req {
    template<class T>
    concept seeker;  // t.seek(int64_t, seek_from) gives expected<uint64_t, error>
}
```

A stream with a position: `t.seek(offset, from)` with an `int64_t` and a [seek_from](../seek_from.md) gives something
convertible to `expected<uint64_t, io::error>`, the position after the seek, counted from the first byte, or the
[error](../error/README.md) of the seek. `T` is the argument as passed: the type, a reference to it, or a `tracked_ptr`,
a `unique_ptr` or a `root_ptr` to it, looked through. A seek is a method; a callable is never a seeker.

It is Go's `io.Seeker`, its `whence` the enumeration `seek_from`. [mixin::seeker](../mixin/seeker/README.md) gives a class
with `seek` the rest of it: `tell`, `size`, `rewind`.

## Satisfied by

- `io::file`, `io::buffer`;
- a class with `seek(int64_t, seek_from)` of that shape, and a `tracked_ptr` to one.

Not by `io::reader`, `io::buffered_reader`, `io::buffered_writer`, `net::connection` or the standard streams.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// The size of any stream with a position, the position kept
expected<uint64_t, io::error> size_of(io::req::seeker auto& s) {
    auto here = s.seek(0, io::seek_from::current);
    if (!here) {
        return here;
    }
    auto end = s.seek(0, io::seek_from::end);
    s.seek(int64_t(*here), io::seek_from::begin);
    return end;
}

int main() {
    io::buffer b("hello, world");
    println("{}", *size_of(b));
    println("{} {}", io::req::seeker<io::buffer>, io::req::seeker<io::reader>);
}
```

Output:

```text
12
true false
```

## See also

- [seek_from](../seek_from.md): where an offset counts from
- [mixin::seeker](../mixin/seeker/README.md): `tell`, `size`, `rewind` over `seek`
- [sgcl::io::req](README.md)
