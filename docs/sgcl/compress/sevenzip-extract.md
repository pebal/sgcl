[sgcl](../README.md) › [compress](README.md) › [sevenzip](sevenzip.md)

# sgcl::compress::sevenzip::extract, async_extract

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    expected<void, error> extract(const string& archive_path, const string& directory,         // (1)
                                  const options& o = {});
    async::task<expected<void, error>> async_extract(string archive_path, string directory,    // (2)
                                                     options o = {}) noexcept;
}
```

Unpacks the archive under the directory, as `7zz x` does, in the archive's order with one decoder a folder. Of the
options it reads the password and the limits; the rest is the writer's.

It writes every entry under the directory, which it makes when it is not there: directories, files with their mode
(the archive's POSIX mode, 0644 when it has none) and modification time, and symbolic links, made last, so that no
file is written through a link the archive made. Anti-items are passed over, and a file already there is written
over.

- **What it refuses.** Every name is checked before anything is written: an absolute one, one with a `..` that
  climbs out, or one with a `\` is `errc::insecure_path` (Go's `ErrInsecurePath`), and nothing is written. A link
  whose target leaves the directory (absolute, or climbing out from the link's own directory) is
  `errc::insecure_path` when it is reached.
- **Too large.** Past `limit.max_size` of the files together (1 GiB by default: an archive comes from outside; 0:
  none) it is `errc::too_large`, and nothing is written.
- **Errors of the data** are those of reading: `errc::password_required`, `errc::wrong_password`, `errc::checksum`
  and the others.

1. Blocks the calling thread.
2. Returns a task that opens the archive as `async_open` does and writes the files on the
   [blocking pool](../async/spawn_blocking.md); the password's keys are made at the call, so the task needs nothing
   of the caller's bytes.

## Parameters

| Parameter | Description |
|---|---|
| `archive_path` | the archive |
| `directory` | where it is unpacked; made when it is not there |
| `o` | the password and the limits ([options](sevenzip-options.md)) |

## Return value

Nothing, or the [error](error/README.md): a name or a link's target that would leave the directory
(`errc::insecure_path`), the files past `max_size` (`errc::too_large`), the archive's own errors as
[open](sevenzip-archive/open.md) and the reads give them, a failure of the file system (`errc::io`).

## Complexity

Linear in the size of the archive's data.

## Exceptions

- (1) What the writes of the files throw; their errors are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    {
        compress::sevenzip::writer w("upload.7z");
        w.add("readme.txt", "hi\n");
        w.add("../evil.sh", "rm\n");
        (void)w.close();
    }
    auto done = compress::sevenzip::extract("upload.7z", "incoming");
    println("{}", done.error().message());
    println("{}", io::exists("incoming/readme.txt"));
    (void)io::remove_all("incoming");
    (void)io::remove("upload.7z");
}
```

Output:

```text
7z: an entry's name leaves the directory: ../evil.sh
false
```

## See also

- [create](sevenzip-create.md): the other way
- [archive](sevenzip-archive/README.md): entries one by one
- [sgcl::compress::sevenzip](sevenzip.md)
