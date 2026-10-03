[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](../sevenzip-writer.md)

# sgcl::compress::sevenzip::writer::add_directory

```cpp
void add_directory(const string& name) noexcept;                            // (1)
void add_directory(const string& name, const entry_info& info) noexcept;    // (2)
```

Writes a directory entry, with no data and in no folder; a trailing `/` of the name is dropped. A failure is kept as
the writer's first error.

1. Modified now, mode 0755.
2. With the times, the mode and the attributes of `info`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the directory's name, UTF-8, `/` between the parts |
| `info` | its times, mode, attributes ([entry_info](../sevenzip-entry_info.md)) |

## Return value

None.

## Complexity

Constant, and the end of the entry before.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::sevenzip::writer w(archive);
    w.add_directory("logs/");
    (void)w.close();

    auto e = compress::sevenzip::archive::from(archive.data())->entries()[0];
    println("{} {} {:o}", e.name, e.is_directory, (e.attributes >> 16) & 0777);
}
```

Output:

```text
logs true 755
```

## See also

- [add](add.md)
- [sgcl::compress::sevenzip::writer](../sevenzip-writer.md)
