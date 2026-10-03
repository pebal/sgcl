[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::clean

```cpp
string clean(const string& path) noexcept;
```

Returns the shortest path equivalent to `path` by lexical processing alone, Go's `filepath.Clean`: each `.` element
dropped, each `..` with the element before it, repeated separators made one and a trailing separator dropped. A
`..` at the start of a relative path stays (there is nothing before it to drop); a `..` right after the root is
dropped, the parent of `/` being `/`. The empty path is `.`. Nothing is asked of the file system, so a `..` after a
symbolic link is resolved as text, not as the link's target.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path to clean |

## Return value

The cleaned path; `.` when nothing is left.

## Complexity

Linear in the length of `path`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* p : {"a//b/../c/", "./a/./b/", "/../x", "../../a/..", "a/b/../../..", ""}) {
        println("\"{}\" -> {}", p, io::path::clean(p));
    }
}
```

Output:

```text
"a//b/../c/" -> a/c
"./a/./b/" -> a/b
"/../x" -> /x
"../../a/.." -> ../..
"a/b/../../.." -> ..
"" -> .
```

## See also

- [join](join.md), [dir](dir.md), [abs](abs.md), [rel](rel.md): the functions that clean their result
- [sgcl::io::path](README.md)
