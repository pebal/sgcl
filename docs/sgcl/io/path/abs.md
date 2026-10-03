[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::abs

```cpp
expected<string, error> abs(const string& p) noexcept;
```

Returns the absolute form of the path, Go's `filepath.Abs`: a relative path joined to the working directory, then
[cleaned](clean.md); an absolute one cleaned alone. The working directory is the one thing asked of the system
(`getcwd`); whether the path exists is not.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the path |

## Return value

The absolute path, cleaned, or the [error](../error/README.md) of `getcwd` (the operation `getcwd`): the working directory
removed, or one longer than 4095 bytes.

## Complexity

Linear in the lengths of the path and of the working directory.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::path::abs("/a/../b").value());
    string full = io::path::abs("notes/../todo.txt");
    string joined = io::path::join(io::working_dir().value(), "todo.txt");
    println("{} {}", io::path::is_abs(full), full == joined);
}
```

Output:

```text
/b
true true
```

## See also

- [rel](rel.md): the path from one path to another
- [working_dir](../working_dir.md): the directory a relative path starts from
- [sgcl::io::path](README.md)
