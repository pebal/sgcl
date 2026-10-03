[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [reader](../tar-reader.md)

# sgcl::compress::tar::reader::reader

```cpp
explicit reader(const io::reader& in) noexcept;
```

Constructs a reader of the archive `in` holds. Nothing is read yet: the first [next](next.md) reads the first
headers. An archive inside gzip, xz or bzip2 is read through that format's reader: `tar::reader(gzip_reader)`.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the reader the archive comes from |

## Complexity

Constant; the input buffer is allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    (void)w.write_header({.name = "a.txt", .size = 1});
    (void)w.write("a");
    (void)w.close();

    io::buffer packed(compress::xz::compress(archive.data()));  // a .tar.xz in memory
    compress::xz::reader unpacked(packed);
    compress::tar::reader r(unpacked);
    println("{}", (*r.next())->name);
}
```

Output:

```text
a.txt
```

## See also

- [next](next.md)
- [sgcl::compress::tar::reader](../tar-reader.md)
