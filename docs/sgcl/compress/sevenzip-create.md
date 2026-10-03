[sgcl](../README.md) › [compress](README.md) › [sevenzip](sevenzip.md)

# sgcl::compress::sevenzip::create, async_create

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    /*(1)*/ expected<void, error> create(const string& directory, const string& archive_path,
                                         const options& o = {});
    /*(2)*/ async::task<expected<void, error>> async_create(string directory, string archive_path,
                                                            options o = {}) noexcept;
}
```

Packs the directory into a 7z file with the [writer](sevenzip-writer.md)'s options: LZMA2 at level 6, solid, the
filters chosen by what each file is, as 7-Zip writes it; a password encrypts it, the header too. The entries are
named from the directory, not with it (`index.html`, `css/`, `css/site.css`), in lexical order, with their mode and
time; a symbolic link is archived as a link; a socket, a device or a fifo is left out. A failure removes the
half-made file.

1. Blocks the calling thread.
2. Returns a task that writes on the [blocking pool](../async/spawn_blocking.md). A password's key is made at the
   call, on the caller's thread (about 12 ms), as `async_extract` makes its keys: the caller's bytes never go to the
   pool, and the task needs nothing of them.

## Parameters

| Parameter | Description |
|---|---|
| `directory` | the directory packed |
| `archive_path` | the archive written |
| `o` | how the archive is written ([options](sevenzip-options.md)) |

## Return value

Nothing, or the [error](error.md): what the [writer](sevenzip-writer.md) refuses, a failure of the file system
(`errc::io`).

## Complexity

Linear in the size of the directory's files.

## Exceptions

- (1) What the reads and writes of the files throw; their errors are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("photos/2026");
    (void)io::write_file("photos/2026/list.txt", "beach.jpg\nhills.jpg\n");
    (void)compress::sevenzip::create("photos", "photos.7z", {.level = 9, .solid = false});

    auto a = compress::sevenzip::archive::open("photos.7z");
    for (auto& e : a->entries()) {
        println("{}", e.name);
    }
    (void)a->close();
    (void)io::remove_all("photos");
    (void)io::remove("photos.7z");
}
```

Output:

```text
2026
2026/list.txt
```

## See also

- [extract](sevenzip-extract.md): the other way
- `tests/compress/files.cpp`: tested both ways against 7-Zip's `7zz`
- [sgcl::compress::sevenzip](sevenzip.md)
