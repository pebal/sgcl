[sgcl](../../README.md) › [io](../README.md) › [mapping](README.md)

# sgcl::io::mapping::writable_data

```cpp
slice<byte> writable_data() const noexcept;
```

Returns the mapped bytes to write into: the same bytes as [data](data.md), as a `slice<byte>`, for a mapping made
with `writable` ([map_options](../map_options.md)). A write lands in the file for a shared mapping, at once for
every reader of the file (one page cache) and on the disk after [flush](flush.md) or when the system writes the page
back; in the program's own copy for a private one. The slice holds the region as its owner, as `data()`'s does.

Asking a read-only mapping for it is a contract violation: a debug build stops on the assertion, any other build
returns an empty slice.

## Parameters

None.

## Return value

The mapped bytes to write into; an empty slice for an empty mapping, once the mapping is [closed](close.md), and
for a read-only mapping.

## Complexity

Constant.

## Exceptions

None.

## Example

Written in place: the file has the bytes after `flush`.

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("greeting.txt", string("hello world"));
    io::mapping m = io::map("greeting.txt", {.writable = true});
    slice<byte> bytes = m.writable_data();
    bytes[0] = byte('H');
    bytes[6] = byte('W');
    (void)m.flush();
    println("{}", io::read_text("greeting.txt").value());
}
```

Output:

```text
Hello World
```

## See also

- [data](data.md): the bytes, read only
- [flush](flush.md): the writes waited for on the disk
- [map_options](../map_options.md): `writable`, `shared`
- [sgcl::io::mapping](README.md)
