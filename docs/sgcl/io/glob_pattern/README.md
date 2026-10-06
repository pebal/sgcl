[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::glob_pattern

```cpp
#include "sgcl/io/glob.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class glob_pattern;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::io::glob_pattern` is a glob pattern of the shell and Python, with `**`, braces and hidden names, where
[path::match](../path/match.md) matches Go's `filepath.Match` patterns without them; compiled: its braces expanded, its components checked, ready to
[match](match.md) a path by itself — a file name from a listing, a path of an archive, a name a server is asked for —
apart from any walk, which [io::glob](../glob.md) does with it. The syntax is the shell's and Python's
`glob(recursive=True)`, and [match](match.md) is held to Python's `glob.translate`:

- `*` any run of characters within a component, `?` one character (a code point of UTF-8), `[...]` one of a class,
  with ranges (`[a-z]`, `[α-ω]`) and a leading `!` or `^` negating it, `\` the next character as it is;
- `**` as a whole component: zero or more components (`a**b` is `a*b`);
- `{a,b}`: either alternative, nested (`{a,b{c,d}}`), expanded when the pattern is compiled (at most 1024
  alternatives); a brace with no comma at its level, or one that does not close, is the character;
- a trailing `/`: directories only;
- the hidden rule: no wildcard matches a name beginning with `.` unless the pattern's component begins with one, or
  [glob_options](../glob_options.md)`::hidden` is set; `.` and `..` never match a wildcard.

Against Go's `filepath.Match` ([path::match](../path/match.md), which stays Go's): `**`, braces, the hidden rule, and
the pattern compiled once. Against Python's `fnmatch`: `^` negates a class as `!` does, and `\` escapes.

## Rules

- A handle of one word, immutable: copies are the same pattern.
- Compiled by the constructor, for a pattern the program writes (a malformed one throws
  `bad_expected_access<io::error>`, [parse](parse.md)'s error inside), or by [parse](parse.md), which returns the
  error, for a pattern from outside.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](glob_pattern.md) | compiles a pattern; the copy is the same one |
| [parse](parse.md) | compiles a pattern, a malformed one an error |

#### Matching

| Function | Description |
|---|---|
| [match](match.md) | checks whether a path matches |

#### Observers

| Function | Description |
|---|---|
| [text](text.md) | the pattern as it was given |
| [is_literal](is_literal.md) | checks whether it names one path only |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::glob_pattern sources("src/**/*.{cpp,h}");
    for (const char* path : {"src/main.cpp", "src/net/http/client.h", "src/net/.cache/x.cpp", "lib/a.cpp"}) {
        println("{} {}", path, sources.match(path));
    }
}
```

Output:

```text
src/main.cpp true
src/net/http/client.h true
src/net/.cache/x.cpp false
lib/a.cpp false
```

## See also

- [glob](../glob.md): the paths of the file system that match
- [glob_options](../glob_options.md): the hidden names
- [path::match](../path/match.md): Go's matching
