[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [entry](../zip-entry.md)

# sgcl::compress::zip::entry::is_local

```cpp
bool is_local() const noexcept;
```

Checks whether the name may be joined to a directory without leaving it: not empty, not absolute, no NUL, no `\`
(APPNOTE's names use `/`), and no `..` that climbs above the start (`a/../b` is local, `a/../..` is not). It is Go's
`filepath.IsLocal`, the rule of [io::path::is_local](../../io/path.md) and of tar's entries too. An archive from
outside names its entries as it likes (`../../etc/passwd`, the *zip slip*); a program that writes entries to disk
checks each name with it, as [extract](../zip-extract.md) does.

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
    compress::zip::entry e;
    for (auto name : {"a/b.txt", "a/../b.txt", "a/../..", "../../etc/passwd", "/etc/passwd",
                      "a\\b"}) {
        e.name = name;
        println("{}: {}", name, e.is_local());
    }
}
```

Output:

```text
a/b.txt: true
a/../b.txt: true
a/../..: false
../../etc/passwd: false
/etc/passwd: false
a\b: false
```

## See also

- [extract](../zip-extract.md): every name checked before anything is written
- [sgcl::compress::zip::entry](../zip-entry.md)
