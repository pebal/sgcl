[sgcl](../README.md) › [io](README.md)

# sgcl::io::path

```cpp
#include "sgcl/io/path.h"   // or "sgcl/io.h"

namespace sgcl::io::path {
    inline constexpr char separator = '/';
    inline constexpr char list_separator = ':';
}
```

`io::path` is the lexical operations on paths, Go's `path/filepath`: the text of a path in the platform's form (`/`
on POSIX) taken apart, joined and cleaned, the file system touched only where the name says so
([abs](path/abs.md), [glob](path/glob.md)). A path is a [string](../core/string.md): a string in (a literal makes
one), a string out. Against `std::filesystem::path`, there is no path type to make and convert, and nothing here
allocates one; against Go, the same names, [stem](path/stem.md) added.

Text is UTF-8: [match](path/match.md) and [glob](path/glob.md) compare code points, so `?` is one character and
`[α-ω]` a range of them. Invalid bytes are not rejected; on POSIX a path is bytes the system does not interpret.

## Rules

- [clean](path/clean.md) is applied by [join](path/join.md), [dir](path/dir.md), [abs](path/abs.md) and
  [rel](path/rel.md); [base](path/base.md), [ext](path/ext.md), [stem](path/stem.md) and [split](path/split.md)
  work on the text as given.
- [match](path/match.md) matches the whole name, element by element: `*` and `?` never match a separator. A
  malformed pattern (an unclosed `[`, a trailing `\`, a reversed range) is `errc::invalid_pattern`, whatever the
  name.
- **A name from outside** — an entry of an archive, the path of a request (which may have been `..%2f` before it
  was decoded), a name a user typed — goes through [under](path/under.md) before it becomes a file's path: the name
  joined to the directory when [is_local](path/is_local.md) says it stays inside, `errc::insecure_path` when it
  does not, nothing joined. The archives' extraction ([tar](../compress/tar.md), [zip](../compress/zip.md),
  [sevenzip](../compress/sevenzip.md)) holds its names to the same rule.

## Member objects

| Constant | Description |
|---|---|
| `separator` | the separator of the elements of a path: `'/'` |
| `list_separator` | the separator of the paths of a list such as `PATH`: `':'` |

## Member functions

| Function | Description |
|---|---|
| [abs](path/abs.md) | the absolute form of a path |
| [base](path/base.md) | the last element |
| [clean](path/clean.md) | the shortest equivalent path |
| [dir](path/dir.md) | everything but the last element |
| [ext](path/ext.md) | the extension |
| [from_slash](path/from_slash.md) | a path with `/` as the separator in the platform's form |
| [glob](path/glob.md) | the paths that match a pattern |
| [is_abs](path/is_abs.md) | checks whether a path is absolute |
| [is_local](path/is_local.md) | checks whether a name stays inside the directory it is joined to |
| [join](path/join.md) | the elements joined and cleaned |
| [match](path/match.md) | checks whether a name matches a shell pattern |
| [rel](path/rel.md) | the path from one path to another |
| [split](path/split.md) | the directory and the file |
| [split_list](path/split_list.md) | the paths of a list such as `PATH` |
| [stem](path/stem.md) | the last element without its extension |
| [to_slash](path/to_slash.md) | a path in the platform's form with `/` as the separator |
| [under](path/under.md) | a name from outside joined to a directory, when it stays inside |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string archive = "/a/b/c.tar.gz";
    println("{}", io::path::clean("a//b/../c/"));
    println("{}", io::path::join("/usr", "local", "bin"));
    println("{} | {}", io::path::base(archive), io::path::dir(archive));
    println("{} | {}", io::path::ext(archive), io::path::stem(archive));
    println("{}", io::path::rel("/a/b", "/a/c/d").value());
    println("{} {}", io::path::match("*.cpp", "main.cpp").value(),
            io::path::match("src/*.cpp", "src/a/b.cpp").value());
    println("{}", io::path::match("[а-я]*", "яблоко").value());
}
```

Output:

```text
a/c
/usr/local/bin
c.tar.gz | /a/b
.gz | c.tar
../c/d
true false
true
```

## See also

- [stat](stat.md), [read_dir](read_dir.md), [walk_dir](walk_dir.md): what is at a path
- [working_dir](working_dir.md), [home_dir](home_dir.md), [temp_dir](temp_dir.md): the directories the platform
  names
- `tests/io/path.cpp`: Go's table for `IsLocal` with `under`, for `Clean`, `join`/`base`/`dir`/`ext`/`stem`/`split`,
  `abs`/`rel`, `match` with classes, escapes and code points, `glob` over a tree
