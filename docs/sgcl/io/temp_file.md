[sgcl](../README.md) › [io](README.md)

# sgcl::io::temp_file, async_temp_file

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    /*(1)*/ expected<file, error> temp_file(const string& dir = {}, const string& pattern = "*");
    /*(2)*/ async::task<expected<file, error>> async_temp_file(const string& dir = {},
                                                               const string& pattern = "*")
                noexcept;
}
```

A new file in `dir` with a name from `pattern`, its last `*` replaced by ten random characters (digits and lower-case
letters), or the ten appended when there is no `*`: `"upload-*.tmp"` gives `upload-k3x9q0d2ma.tmp`. The file is
created with `exclusive` ([open_flags](open_flags.md)), so it is new, the names tried again on a collision; it is
opened for reading and writing, with the permissions `0600` (before the umask). The caller removes it. Go's
`os.CreateTemp`.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../async/spawn_blocking.md), as [create](create.md).

## Parameters

| Parameter | Description |
|---|---|
| `dir` | the directory of the file; the system's temporary directory when empty, `$TMPDIR`, else `/tmp` ([temp_dir](temp_dir.md)) |
| `pattern` | the name, its last `*` replaced by the random characters |

## Return value

The file, its [path](file/path.md) `dir` joined with the name, or the [error](error.md) of [open](open.md)
(`is_not_found()` for a `dir` that is not there, `is_permission()`); after 10000 names that all exist, an error
with the code `std::errc::file_exists`, its operation `temp_file` and its path `pattern`.

## Complexity

As [open](open.md), once for each name tried: one, and more only when a name drawn exists already.

## Exceptions

- (1) `std::system_error` when `std::random_device`, which seeds the names on the first call of a thread, cannot
  give the seed.
- (2) None: the task's own exceptions are the task's.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file upload = io::temp_file(".", "upload-*.tmp").value();
    println("{}", upload.path());
    upload.write("received");
    upload.rewind();
    println("{}", *upload.read_all_text());
    upload.close();
    io::remove(upload.path());
}
```

Sample output:

```text
upload-k3x9q0d2ma.tmp
received
```

## See also

- [make_temp_dir](make_temp_dir.md): a new directory the same way
- [temp_dir](temp_dir.md): the system's temporary directory
- [remove](remove.md): what the caller does after
- [sgcl::io::file](file.md)
