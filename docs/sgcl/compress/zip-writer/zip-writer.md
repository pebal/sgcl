[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](../zip-writer.md)

# sgcl::compress::zip::writer::writer

```cpp
explicit writer(const io::writer& out) noexcept;
```

Constructs a writer of an archive into `out`. Nothing is written yet: the first entry's local header goes with its
`create`.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the writer the archive goes to: a file, a buffer, a socket; it is never asked to seek |

## Complexity

Constant; the writer's state is allocated.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::zip::writer w(archive);
    (void)w.close();
    println("{} bytes", archive.size());  // an empty archive: the end record alone
}
```

Output:

```text
22 bytes
```

## See also

- [create](create.md)
- [sgcl::compress::zip::writer](../zip-writer.md)
