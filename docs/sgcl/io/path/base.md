[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::base

```cpp
string base(const string& path) noexcept;
```

Returns the last element of `path`, Go's `filepath.Base`: trailing separators are dropped first, then everything up
to the last separator. A path of separators alone gives `/`, and the empty path gives itself, where Go's gives `.`.
The text is taken as given, not [cleaned](clean.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path |

## Return value

The last element; `/` for a path of separators alone; an empty string for an empty path.

## Complexity

Linear in the length of `path`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* p : {"/a/b/c.tar.gz", "a/b/", "c", "/", ""}) {
        println("\"{}\" -> \"{}\"", p, io::path::base(p));
    }
}
```

Output:

```text
"/a/b/c.tar.gz" -> "c.tar.gz"
"a/b/" -> "b"
"c" -> "c"
"/" -> "/"
"" -> ""
```

## See also

- [dir](dir.md): everything but the last element
- [stem](stem.md), [ext](ext.md): the last element without its extension, and the extension
- [split](split.md): the directory and the file at once
- [sgcl::io::path](../path.md)
