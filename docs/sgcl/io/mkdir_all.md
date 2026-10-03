[sgcl](../README.md) › [io](README.md)

# sgcl::io::mkdir_all, async_mkdir_all

```cpp
#include "sgcl/io/fs.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ expected<void, error> mkdir_all(const string& path,
                                            permissions p = permissions(0777)) noexcept;
    /*(2)*/ async::task<expected<void, error>> async_mkdir_all(const string& path,
                                                             permissions p = permissions(0777))
                noexcept;
}
```

Makes the directory and every missing parent, `mkdir -p`, Go's `os.MkdirAll`. The path is [cleaned](path/clean.md)
first; a directory of the chain that exists is left as it is, so the call is idempotent: a directory already there
is no error. Each directory made gets `p`, less the umask.

1. Waits on the calling thread.
2. The same as a task: the call runs on the [blocking pool](../async/spawn_blocking.md), so that a task holds no worker while the disk works.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the directory to make, with its parents |
| `p` | the permissions of the directories made, before the umask; `0777` by default |

## Return value

Nothing, or the [error](error.md): the error of the `mkdir` of the first element that could not be made, or
`std::errc::not_a_directory` when something other than a directory is at the path; the operation is `mkdir`.

## Complexity

Linear in the number of elements of the path: a call to the system for each.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::mkdir_all("site/static/css").has_value());
    println("{}", io::mkdir_all("site/static").has_value());
    println("{}", io::is_directory("site/static/css"));
    (void)io::write_file("site/index.html", "<p>");
    println("{}", io::mkdir_all("site/index.html").error().message());
}
```

Output:

```text
true
true
true
mkdir site/index.html: Not a directory
```

## See also

- [mkdir](mkdir.md): one directory
- [remove_all](remove_all.md): a tree removed
- [cache_dir](cache_dir.md), [config_dir](config_dir.md): the directories a program keeps its own under
