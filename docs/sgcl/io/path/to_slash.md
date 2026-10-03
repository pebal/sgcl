[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::to_slash

```cpp
string to_slash(const string& p) noexcept;
```

Returns the path with each of the platform's separators made `/`, Go's `filepath.ToSlash`: a path of the file system
made the form of a URL or of an archive's entry. On POSIX the separator is `/`, and the path is returned as it is.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the path in the platform's form |

## Return value

The path with `/` as its separator.

## Complexity

Linear in the length of `p`: the copy.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string file = io::path::join("site", "img", "logo.png");
    println("/{}", io::path::to_slash(io::path::rel("site", file).value()));
}
```

Output:

```text
/img/logo.png
```

## See also

- [from_slash](from_slash.md): the other way
- [sgcl::io::path](../path.md)
