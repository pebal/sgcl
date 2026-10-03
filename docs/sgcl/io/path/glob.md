[sgcl](../../README.md) › [io](../README.md) › [path](README.md)

# sgcl::io::path::glob

```cpp
expected<vector<string>, error> glob(const string& pattern) noexcept;
```

Returns the paths of the file system that match the pattern, Go's `filepath.Glob`, with the rules of
[match](match.md). The directory part may hold patterns of its own (`src/*/*.cpp`): each directory that matches is
read in turn. The paths are sorted within each directory, and are relative when the pattern is. A directory that
cannot be read is skipped, as Go skips it; a pattern without a meta character (`*`, `?`, `[`, `\`) names the file,
which is the result when it exists; an empty pattern names none.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern of the paths |

## Return value

The matching paths, empty when there are none, or an [error](../error/README.md) with `errc::invalid_pattern` for a
malformed pattern; the error's operation is `glob`, its path the pattern.

## Complexity

Linear in the number of entries of the directories read, and the sort of the matches of each.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("src/net");
    (void)io::mkdir_all("src/core");
    for (const char* name : {"src/main.cpp", "src/net/socket.cpp", "src/net/socket.h",
                             "src/core/vector.cpp"}) {
        (void)io::write_file(name, string());
    }
    println("{}", io::path::glob("src/*/*.cpp").value());
    println("{}", io::path::glob("src/*.cpp").value());
    println("{}", io::path::glob("src/net/socket.h").value());
    println("{}", io::path::glob("docs/*.md").value());
    println("{}", io::path::glob("src/[").error().message());
}
```

Output:

```text
["src/core/vector.cpp", "src/net/socket.cpp"]
["src/main.cpp"]
["src/net/socket.h"]
[]
glob src/[: invalid pattern
```

## See also

- [match](match.md): the rules of a pattern
- [read_dir](../read_dir.md), [walk_dir](../walk_dir.md): the entries of a directory, and of a tree
- [sgcl::io::path](README.md)
