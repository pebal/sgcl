[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [entry](README.md)

# sgcl::compress::tar::entry::is_local

```cpp
bool is_local() const noexcept;
```

Checks whether the name, and the link's target where there is one, stay inside the directory the archive is unpacked
to: Go's `filepath.IsLocal`, the rule of [io::path::is_local](../../io/path/README.md). Not empty, not absolute, no `..` that
climbs out of the directory, no backslash (a separator on Windows). A symlink's target is taken from the directory
of the link, a hard link's from the archive's root. An archive from outside names its entries as it likes: a program
writing them to disk checks each with this first, as [extract](../tar-extract.md) does.

## Parameters

None.

## Return value

`true` when the name and the link's target stay inside the directory.

## Complexity

Linear in the length of the name and the target.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::tar::entry e;
    for (auto name : {"a/b.txt", "a/../b.txt", "../b.txt", "/etc/passwd", "a\\b.txt"}) {
        e.name = name;
        println("{}: {}", name, e.is_local());
    }
    e = {.name = "docs/latest", .link_name = "../v2", .type = compress::tar::kind::symlink};
    println("docs/latest -> ../v2: {}", e.is_local());
    e.link_name = "../../etc";
    println("docs/latest -> ../../etc: {}", e.is_local());
}
```

Output:

```text
a/b.txt: true
a/../b.txt: true
../b.txt: false
/etc/passwd: false
a\b.txt: false
docs/latest -> ../v2: true
docs/latest -> ../../etc: false
```

## See also

- [extract](../tar-extract.md): every name checked before anything is written
- [sgcl::compress::tar::entry](README.md)
