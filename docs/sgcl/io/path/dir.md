[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::dir

```cpp
string dir(const string& path) noexcept;
```

Returns everything but the last element of `path`, [cleaned](clean.md), Go's `filepath.Dir`: the text up to the last
separator. A path without a separator is in the current directory, `.`. A trailing separator makes the last element
empty, so `dir("a/b/")` is `a/b`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path |

## Return value

The directory of the path, cleaned; `.` when the path has no separator.

## Complexity

Linear in the length of `path`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* p : {"/a/b/c.tar.gz", "a/b/", "a//b/../c", "c", "/", ""}) {
        println("\"{}\" -> {}", p, io::path::dir(p));
    }
}
```

Output:

```text
"/a/b/c.tar.gz" -> /a/b
"a/b/" -> a/b
"a//b/../c" -> a
"c" -> .
"/" -> /
"" -> .
```

## See also

- [base](base.md): the last element
- [split](split.md): the directory as written and the file
- [sgcl::io::path](README.md)
