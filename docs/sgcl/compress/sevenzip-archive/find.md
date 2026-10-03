[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](README.md)

# sgcl::compress::sevenzip::archive::find

```cpp
optional<entry> find(const string& name) const noexcept;
```

Finds the first entry of the name, in the archive's order.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the entry's name, UTF-8, `/` between the parts |

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
    compress::sevenzip::writer w(archive);
    w.add("config.json", "{}");
    (void)w.close();

    auto a = compress::sevenzip::archive::from(archive.data());
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
- [sgcl::compress::sevenzip::archive](README.md)
