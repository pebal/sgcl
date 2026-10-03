[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::is_local

```cpp
bool is_local(const string& name) noexcept;
```

Checks whether the name may be joined to a directory without leaving it, Go's `filepath.IsLocal`: it is not empty,
not absolute, holds no NUL and no `\` (a separator on Windows, and not one in a zip or a tar), and no `..` in it
climbs above its start once the name is taken lexically: `a/../b` is local, `a/../..` is not. Nothing is asked of
the file system, so a symbolic link inside the directory is not looked at.

A name from outside the program — an entry of an archive, the path of a request, which may have been `..%2f` before
it was decoded — is checked so before it becomes a file's path; [under](under.md) checks and joins in one call.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name to check |

## Return value

`true` when the name, joined to a directory, stays inside it.

## Complexity

Linear in the length of `name`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* name : {"a/../b", "a/./b", "a/../..", "..", "/etc/passwd", "a\\b", ""}) {
        println("\"{}\" {}", name, io::path::is_local(name));
    }
}
```

Output:

```text
"a/../b" true
"a/./b" true
"a/../.." false
".." false
"/etc/passwd" false
"a\b" false
"" false
```

## See also

- [under](under.md): the name joined to the directory when it is local
- [tar](../../compress/tar.md), [zip](../../compress/zip.md), [sevenzip](../../compress/sevenzip.md): the extraction
  that holds the names of an archive to this rule
- [sgcl::io::path](README.md)
