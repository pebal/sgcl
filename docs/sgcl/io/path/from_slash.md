[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::from_slash

```cpp
string from_slash(const string& p) noexcept;
```

Returns the path with each `/` made the platform's separator, Go's `filepath.FromSlash`: a path in the form of a URL
or of an archive's entry made a path of the file system. On POSIX the separator is `/`, and the path is returned as
it is.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the path with `/` as its separator |

## Return value

The path in the platform's form.

## Complexity

Linear in the length of `p`: the copy.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string entry = "docs/sgcl/README.md";  // as an archive names it
    println("{}", io::path::join("unpacked", io::path::from_slash(entry)));
}
```

Output:

```text
unpacked/docs/sgcl/README.md
```

## See also

- [to_slash](to_slash.md): the other way
- [sgcl::io::path](../path.md)
