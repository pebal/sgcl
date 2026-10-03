[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::split_list

```cpp
vector<string> split_list(const string& path_list) noexcept;
```

Splits a list of paths joined by `list_separator` (`:`), as `PATH` is, Go's `filepath.SplitList`. Empty elements
are skipped, where Go keeps them: an empty list, and a list of separators alone, give no element.

## Parameters

| Parameter | Description |
|---|---|
| `path_list` | the list of paths |

## Return value

The paths of the list, in order, without the empty ones.

## Complexity

Linear in the length of `path_list`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::path::split_list("/usr/local/bin::/usr/bin:/bin:"));
    println("{}", io::path::split_list("").size());
}
```

Output:

```text
["/usr/local/bin", "/usr/bin", "/bin"]
0
```

## See also

- [getenv](../getenv.md): the text of `PATH`
- [look_path](../look_path.md): the executable a name stands for, searched in `PATH`
- [sgcl::io::path](README.md)
