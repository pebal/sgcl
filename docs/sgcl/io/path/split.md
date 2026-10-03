[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::split

```cpp
pair<string, string> split(const string& path) noexcept;
```

Splits `path` after its last separator, Go's `filepath.Split`: the directory with its trailing separator as
written, and the file after it. Nothing is cleaned, so the two joined back as text are `path` again. A path without
a separator is a file alone, with an empty directory.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path |

## Return value

A pair: the text up to the last separator, the separator included, and the text after it.

## Complexity

Linear in the length of `path`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* p : {"/a/b/c.tar.gz", "a//b", "a/b/", "c"}) {
        auto [directory, file] = io::path::split(p);
        println("\"{}\" -> \"{}\" \"{}\"", p, directory, file);
    }
}
```

Output:

```text
"/a/b/c.tar.gz" -> "/a/b/" "c.tar.gz"
"a//b" -> "a//" "b"
"a/b/" -> "a/b/" ""
"c" -> "" "c"
```

## See also

- [dir](dir.md), [base](base.md): the two halves, the directory cleaned
- [split_list](split_list.md): a list of paths taken apart
- [sgcl::io::path](../path.md)
