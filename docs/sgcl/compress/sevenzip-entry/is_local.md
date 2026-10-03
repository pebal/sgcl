[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [entry](../sevenzip-entry.md)

# sgcl::compress::sevenzip::entry::is_local

```cpp
bool is_local() const noexcept;
```

Checks whether the name may be joined to a directory without leaving it: not empty, not absolute, no NUL, no `\` (7z's
names use `/`), no `..` that climbs above the start — Go's `filepath.IsLocal`, the rule of [zip](../zip.md) and
[tar](../tar.md), and the check a program makes before joining a name to a directory.
[extract](../sevenzip-extract.md) makes it for every name before anything is written.

## Parameters

None.

## Return value

`true` when the name stays inside the directory it is joined to.

## Complexity

Linear in the length of the name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::sevenzip::entry e;
    for (auto name : {"docs/a.txt", "docs/../a.txt", "../a.txt", "/a.txt"}) {
        e.name = name;
        println("{}: {}", name, e.is_local());
    }
}
```

Output:

```text
docs/a.txt: true
docs/../a.txt: true
../a.txt: false
/a.txt: false
```

## See also

- [extract](../sevenzip-extract.md)
- [sgcl::compress::sevenzip::entry](../sevenzip-entry.md)
