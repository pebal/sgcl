[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [writer](../tar-writer.md)

# sgcl::compress::tar::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;
```

Constructs a writer of an archive into `out`. Nothing is written yet. An archive inside gzip or xz is written
through that format's writer: `tar::writer(gzip_writer)`, the gzip writer closed after the tar writer.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the archive goes to |

## Complexity

Constant.

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
    (void)w.write_header({.name = "empty/", .type = compress::tar::kind::directory});
    (void)w.close();
    println("{} bytes", archive.size());  // a header and two blocks of zeros, 512 bytes each
}
```

Output:

```text
1536 bytes
```

## See also

- [write_header](write_header.md)
- [sgcl::compress::tar::writer](../tar-writer.md)
