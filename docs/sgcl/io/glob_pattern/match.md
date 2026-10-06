[sgcl](../../README.md) › [io](../README.md) › [glob_pattern](README.md)

# sgcl::io::glob_pattern::match

```cpp
bool match(const string& path) const noexcept;
```

Checks whether `path` matches the pattern: its components, separated by `/`, against the pattern's — a wildcard
component against one, `**` against any number of them, none hidden unless the
[options](../glob_options.md) say so. A path that begins with `/` matches a pattern that does, and only one. A path
ending in `/` is a directory: it is matched only by a pattern ending in `/` (which asks for one) or in `**` (`a/` by
`a/**`, the directory itself, as [glob](../glob.md) writes it). The path is taken as written, not cleaned: an empty
component (`a//b`) is skipped, `.` and `..` match only themselves. What [glob](../glob.md) tests each path with, and
what Python's `glob.translate` says, case for case.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path, its separators `/` |

## Return value

`true` when the path matches.

## Complexity

Linear in the length of the path times the pattern's for a pattern without `**`, quadratic in the number of
components with it (each pair of a `**` and a component tried once), for each alternative of its braces. Nothing is
allocated for a pattern without `**`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::glob_pattern tests("**/*_test.{cpp,go}");
    for (const char* path : {"net/http/client_test.cpp", "client_test.go", "net/client.cpp", ".git/x_test.go"}) {
        println("{} {}", path, tests.match(path));
    }
    io::glob_pattern dirs("build/**/");
    println("{} {}", dirs.match("build/debug/"), dirs.match("build/debug"));
}
```

Output:

```text
net/http/client_test.cpp true
client_test.go true
net/client.cpp false
.git/x_test.go false
true false
```

## See also

- [glob](../glob.md): the paths of the file system that match
- [path::match](../path/match.md): Go's matching, component by component
- [sgcl::io::glob_pattern](README.md)
