[sgcl](../README.md) › [compress](README.md) › [tar](tar.md)

# sgcl::compress::tar::extract, async_extract

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    expected<void, error> extract(const string& archive_path, const string& directory,         // (1)
                                  const options& o = {});
    async::task<expected<void, error>> async_extract(string archive_path, string directory,    // (2)
                                                     options o = {}) noexcept;
}
```

Unpacks the archive under the directory, as `tar -x` does: `tar::extract("site.tar.gz", "site")`. gzip, xz and
bzip2 around the archive are read by its first bytes, whatever its name.

**Every name is checked before anything is written**: an entry, or a link's target, that would leave the directory
(the rule of [io::path::is_local](../io/path.md), [is_local](tar-entry/is_local.md)) is `errc::insecure_path`, and
nothing is written; so is a total size past the options' `max_size` (`errc::too_large`), 1 GiB unless set, as the
module's [limits](limits.md) have it for `decompress`: `tar -x` has no bound, this one has, since an archive comes
from outside. The archive is read twice for this, once for the names and once for the data. Then the directory and
what the archive holds: directories (made with at least `rwx` for the owner), files with their mode and time, and
symbolic and hard links, made last, so that no file is written through a link the archive made. A device or a fifo
is left out, as Go's tools leave them; a file there already is written over.

1. Blocks the calling thread.
2. Returns a task that runs the work on the [blocking pool](../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `archive_path` | the archive: `.tar`, or with gzip, xz or bzip2 around it |
| `directory` | where it is unpacked; made when it is not there |
| `o` | `max_size`, the bound on the files' bytes together ([options](tar-options.md)) |

## Return value

Nothing, or the [error](error.md): a name that would leave the directory (`errc::insecure_path`), the files past
`max_size` (`errc::too_large`), the archive's own errors as the [reader](tar-reader.md) gives them, a failure of the
file system (`errc::io`).

## Complexity

Linear in the size of the archive, read twice.

## Exceptions

- (1) What the reads and writes of the files throw; their errors are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // an archive from outside, with a name that climbs out
    io::writer file = *io::create("upload.tar");
    compress::tar::writer w(file);
    (void)w.write_header({.name = "readme.txt", .size = 3});
    (void)w.write("hi\n");
    (void)w.write_header({.name = "../evil.sh", .size = 3});
    (void)w.write("rm\n");
    (void)w.close();
    (void)file.close();

    auto done = compress::tar::extract("upload.tar", "incoming");
    println("{}", done.error().message());
    println("{} {}", io::exists("incoming/readme.txt"), io::exists("evil.sh"));
    (void)io::remove("upload.tar");
}
```

Output:

```text
tar: an entry's name or link leaves the directory: ../evil.sh
false false
```

## See also

- [create](tar-create.md): the other way
- [reader](tar-reader.md): an archive entry by entry
- `tests/compress/files.cpp`: tested both ways against bsdtar
- [sgcl::compress::tar](tar.md)
