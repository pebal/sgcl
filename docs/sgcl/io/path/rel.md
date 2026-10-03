[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::rel

```cpp
expected<string, error> rel(const string& base_path, const string& target) noexcept;
```

Returns the path that leads from `base_path` to `target`, Go's `filepath.Rel`: both are [cleaned](clean.md) first,
the elements they share at the start are dropped, and a `..` stands for each element of the base left over. Joined
to `base_path`, the result names `target`. It is lexical: when the answer cannot be found without the file system
it is an error — one path absolute and the other relative, or a base that keeps a `..` past the part the two share
(from `../z` the way back to `.` would need the name of the directory above).

## Parameters

| Parameter | Description |
|---|---|
| `base_path` | the path the result starts from |
| `target` | the path the result leads to |

## Return value

The relative path, `.` when the two are the same, or an [error](../error.md) with `errc::invalid_path`, the
operation `rel` and `target` as its path, when it cannot be found lexically.

## Complexity

Linear in the lengths of the two paths.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    const char* pairs[][2] = {{"/a/b", "/a/c/d"}, {"a/b", "."}, {"a/b", "a/b"}, {"..", "../a"},
                              {"a", "/b"}, {"../z", "."}};
    for (auto [from, to] : pairs) {
        auto way = io::path::rel(from, to);
        println("{} to {}: {}", from, to, way ? *way : way.error().message());
    }
}
```

Output:

```text
/a/b to /a/c/d: ../c/d
a/b to .: ../..
a/b to a/b: .
.. to ../a: a
a to /b: rel /b: invalid path
../z to .: rel .: invalid path
```

## See also

- [abs](abs.md): the absolute form of a path
- [join](join.md): the base and the result joined back
- [sgcl::io::path](../path.md)
