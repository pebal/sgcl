[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::ext

```cpp
string ext(const string& path) noexcept;
```

Returns the extension of `path`, Go's `filepath.Ext`: the text from the last dot of the last element, the dot
included. A dot in a directory's name does not count, and a name that begins with its only dot is all extension
(`.bashrc`). The text is taken as given, not [cleaned](clean.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path |

## Return value

The extension with its dot; an empty string when the last element has no dot.

## Complexity

Linear in the length of the last element.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* p : {"/a/b/c.tar.gz", "notes.txt", ".bashrc", "a.d/b", "Makefile"}) {
        println("\"{}\" -> \"{}\"", p, io::path::ext(p));
    }
}
```

Output:

```text
"/a/b/c.tar.gz" -> ".gz"
"notes.txt" -> ".txt"
".bashrc" -> ".bashrc"
"a.d/b" -> ""
"Makefile" -> ""
```

## See also

- [stem](stem.md): the last element without the extension
- [base](base.md): the last element
- [sgcl::io::path](README.md)
