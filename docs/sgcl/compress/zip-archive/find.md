[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](../zip-archive.md)

# sgcl::compress::zip::archive::find

```cpp
optional<entry> find(const string& name) const noexcept;
```

Finds the first entry of the name, in the order of the central directory.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the entry's name, as the archive has it |

## Return value

A copy of the entry, or `nullopt` when no entry has the name.

## Complexity

Linear in the number of entries.

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
    (void)w.add("config.json", "{}");
    (void)w.close();

    auto a = compress::zip::archive::from(archive.data());
    if (auto e = a->find("config.json")) {
        println("{} bytes", e->size);
    }
    println("{}", a->find("missing.json").has_value());
}
```

Output:

```text
2 bytes
false
```

## See also

- [entries](entries.md)
- [sgcl::compress::zip::archive](../zip-archive.md)
