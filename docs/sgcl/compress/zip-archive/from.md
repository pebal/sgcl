[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](README.md)

# sgcl::compress::zip::archive::from

```cpp
static expected<archive, error> from(const slice<const byte>& data) noexcept;
```

Opens an archive in memory: an upload, a resource, a response's body. The central directory is read as
[open](open.md) reads it from a file, and the entries are read from the bytes when they are asked for. A slice of a
managed buffer (a `vector<byte>`, an `io::buffer`'s data) keeps the buffer alive while the archive is used; a buffer
of unmanaged memory is the caller's to keep.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the archive |

## Return value

The archive, or the [error](../error/README.md) as [open](open.md) gives it.

## Complexity

Linear in the size of the central directory.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer upload;
    compress::zip::writer w(upload);
    (void)w.add("a.txt", "first\n");
    (void)w.add("b.txt", "second\n");
    (void)w.close();

    auto a = compress::zip::archive::from(upload.data());
    for (auto& e : a->entries()) {
        print("{}: {}", e.name, string(slice<const byte>(*a->read(e))));
    }
    println("{}", compress::zip::archive::from(slice<const byte>()).error().message());
}
```

Output:

```text
a.txt: first
b.txt: second
offset 0: zip: not a zip file
```

## See also

- [open](open.md): an archive of a file
- [sgcl::compress::zip::archive](README.md)
