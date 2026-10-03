[sgcl](../README.md) › [compress](README.md) › [zip](zip.md)

# sgcl::compress::zip::extract, async_extract

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    expected<void, error> extract(const string& archive_path, const string& directory,         // (1)
                                  const options& o = {});
    async::task<expected<void, error>> async_extract(string archive_path, string directory,    // (2)
                                                     options o = {}) noexcept;
}
```

Unpacks the archive under the directory, as `unzip` does: `zip::extract("site.zip", "site")`.

**Every name is checked before anything is written**: an entry, or a link's target, that would leave the directory
(the rule of [io::path::is_local](../io/path/README.md), [is_local](zip-entry/is_local.md)) is `errc::insecure_path`, and
nothing is written; so is a total size past the options' `max_size` (`errc::too_large`), 1 GiB unless set, as
`decompress` has it, since an archive comes from outside. Then the directory and what the archive holds:
directories, files with their mode and time, and symbolic links, made last, their targets kept inside. A file there
already is written over.

1. Blocks the calling thread.
2. Returns a task that runs the work on the [blocking pool](../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `archive_path` | the archive |
| `directory` | where it is unpacked; made when it is not there |
| `o` | `max_size`, the bound on the files' bytes together ([options](zip-options.md)) |

## Return value

Nothing, or the [error](error/README.md): a name that would leave the directory (`errc::insecure_path`), the files past
`max_size` (`errc::too_large`), the archive's own errors as the [archive](zip-archive/README.md) gives them, a failure of
the file system (`errc::io`).

## Complexity

Linear in the size of the files.

## Exceptions

- (1) What the reads and writes of the files throw; their errors are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // an archive from outside, with a name that climbs out (the zip slip)
    io::writer file = *io::create("upload.zip");
    compress::zip::writer w(file);
    (void)w.add("readme.txt", "hi\n");
    (void)w.add("../../evil.sh", "rm\n");
    (void)w.close();
    (void)file.close();

    auto done = compress::zip::extract("upload.zip", "incoming");
    println("{}", done.error().message());
    println("{}", io::exists("incoming/readme.txt"));
    (void)io::remove("upload.zip");
}
```

Output:

```text
zip: an entry's name leaves the directory: ../../evil.sh
false
```

## See also

- [create](zip-create.md): the other way
- [archive](zip-archive/README.md): entries one by one
- `tests/compress/files.cpp`: tested both ways against Info-ZIP's `zip` and `unzip`
- [sgcl::compress::zip](zip.md)
