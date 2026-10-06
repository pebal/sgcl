[sgcl](../README.md) › [io](README.md)

# sgcl::io::glob, async_glob

```cpp
#include "sgcl/io/glob.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<vector<string>, error> glob(const string& pattern, const glob_options& options = {}) noexcept;    // (1)
    expected<vector<string>, error> glob(const glob_pattern& pattern) noexcept;                                // (2)
    async::task<expected<vector<string>, error>> async_glob(const string& pattern,                             // (3)
                                                            const glob_options& options = {}) noexcept;
}
```

The paths that match a glob pattern as the shell and Python's `glob(recursive=True)` find them, with `**`, braces
and hidden names, where [path::glob](path/glob.md) finds those of Go's `filepath.Glob` without them: `**` as a whole
component stands for zero or more directories, `{a,b}` for either alternative, `*`, `?` and `[...]` for parts of one
name ([glob_pattern](glob_pattern/README.md) has the whole syntax). The walk starts at the working directory, or at the
root for a pattern that begins with `/`, and reads only the directories the pattern can reach: a literal component
is joined without a listing, a wildcard lists its directory once, `**` the tree under it.

1. The pattern compiled and walked.
2. A pattern compiled already.
3. (1) for a task, on the [blocking pool](../async/spawn_blocking.md): the directories are read as files are.

What a walk yields is Python's, path for path: `a/**` yields `a/`, the directory itself with its slash, and everything
under it; a pattern ending in `/` yields directories only, each with its slash; a wildcard passes over a name that
begins with `.` unless the pattern's component begins with one, or [glob_options](glob_options.md)`::hidden` is set,
and `**` enters no hidden directory then. `**` enters no link to a directory (no loop is possible; Python follows
them), while a wildcard in the middle of a pattern goes through one, as in Go and Python. Against Go's
`filepath.Glob` ([path::glob](path/glob.md), which stays Go's): `**`, braces and the hidden rule.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern, its separators `/` |
| `options` | whether wildcards match hidden names |

## Return value

The paths, sorted by their bytes, each once (Python's come in the directory's order), written as the pattern writes
them: relative to the working directory for a relative pattern. A directory that cannot be read is skipped, as Go and
Python skip it; nothing matching is an empty vector. Or the [error](error/README.md) `errc::invalid_pattern`, its
operation `glob` and its path the pattern, for a malformed pattern (a class that does not close or holds a range
backwards, a `\` at the end, braces of more than 1024 alternatives).

## Complexity

Linear in the entries of the directories the pattern reaches, times the length of a name; `**` reaches every
directory under it.

## Exceptions

- (1–2) None.
- (3) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::mkdir_all("src/net/http");
    for (const char* name : {"src/main.cpp", "src/main.h", "src/net/socket.cpp", "src/net/http/client.cpp",
                             "src/net/http/client.h", "src/.cache.cpp", "README.md"}) {
        io::write_file(name, "");
    }
    vector<string> sources = io::glob("src/**/*.{cpp,h}").value();
    for (const string& found : sources) {
        println("{}", found);
    }
    println("{}", io::glob("src/**/").value().size());
    println("{}", io::glob("src/[").error().message());
}
```

Output:

```text
src/main.cpp
src/main.h
src/net/http/client.cpp
src/net/http/client.h
src/net/socket.cpp
3
glob src/[: invalid pattern
```

## See also

- [glob_pattern](glob_pattern/README.md): a pattern compiled, its match apart from a walk
- [path::glob](path/glob.md), [path::match](path/match.md): Go's, without `**` and braces
- [walk_dir](walk_dir.md): every entry under a directory
