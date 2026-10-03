[sgcl](../README.md) › [io](README.md)

# sgcl::io::make_temp_dir, async_make_temp_dir

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ expected<string, error> make_temp_dir(const string& dir = {},
                                                  const string& pattern = "*");
    /*(2)*/ async::task<expected<string, error>> async_make_temp_dir(const string& dir = {},
                                                                     const string& pattern = "*")
                noexcept;
}
```

A new directory in `dir` with a name from `pattern`, its last `*` replaced by ten random characters (digits and
lower-case letters), or the ten appended when there is no `*`: `"build-*"` gives `build-0q8s1mzk4c`. It is made with
the permissions `0700` (before the umask), the names tried again when one exists. The caller removes it. Go's
`os.MkdirTemp`; [temp_dir](temp_dir.md) is the system's directory itself.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md), as [create](create.md).

## Parameters

| Parameter | Description |
|---|---|
| `dir` | the directory to make it in; the system's temporary directory when empty, `$TMPDIR`, else `/tmp` |
| `pattern` | the name, its last `*` replaced by the random characters |

## Return value

The path of the directory, `dir` joined with the name, or the [error](error.md) of [mkdir](mkdir.md)
(`is_not_found()` for a `dir` that is not there, `is_permission()`); after 10000 names that all exist, an error
with the code `std::errc::file_exists`, its operation `make_temp_dir` and its path `pattern`.

## Complexity

One `mkdir` for each name tried: one, and more only when a name drawn exists already.

## Exceptions

- (1) `std::system_error` when `std::random_device`, which seeds the names on the first call of a thread, cannot
  give the seed.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string dir = io::make_temp_dir(".", "build-*").value();
    println("{}", dir);
    io::file object = io::temp_file(dir, "*.o").value();
    println("{}", object.path());
    object.close();
    io::remove_all(dir);
    println("{}", io::exists(dir));
}
```

Sample output:

```text
build-0q8s1mzk4c
build-0q8s1mzk4c/7dk2l0x9aq.o
false
```

## See also

- [temp_file](temp_file.md): a new file the same way
- [temp_dir](temp_dir.md): the system's temporary directory
- [remove_all](remove_all.md): what the caller does after
- [sgcl::io::file](file.md)
