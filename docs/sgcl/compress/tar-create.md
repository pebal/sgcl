[sgcl](../README.md) › [compress](README.md) › [tar](tar.md)

# sgcl::compress::tar::create, async_create

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    expected<void, error> create(const string& directory, const string& archive_path,         // (1)
                                 const options& o = {});
    async::task<expected<void, error>> async_create(string directory, string archive_path,    // (2)
                                                    options o = {}) noexcept;
}
```

Packs the directory into an archive, as `tar -c` does: `tar::create("site", "site.tar.gz")`. The name says what wraps
the archive: `.tar.gz` and `.tgz` gzip, `.tar.xz` and `.txz` xz, at the options' level, anything else none;
`.tar.bz2` is `errc::unsupported` (bzip2 is read, not written). The entries are named from the directory, not with it
(`index.html`, `css/`, `css/site.css`), as Go's `AddFS` names them, in lexical order, with their mode and time; a
symbolic link is archived as a link, not followed; a socket, a device or a fifo is left out. A file that changes size
while it is archived is `errc::invalid_argument`. A failure removes the half-made archive.

1. Blocks the calling thread.
2. Returns a task that runs the work on the [blocking pool](../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `directory` | the directory packed |
| `archive_path` | the archive written; its name says what wraps it |
| `o` | `level`, the level of gzip or xz ([options](tar-options.md)) |

## Return value

Nothing, or the [error](error.md): `.tar.bz2` (`errc::unsupported`), a file that changed size
(`errc::invalid_argument`), what the [writer](tar-writer.md) refuses, a failure of the file system (`errc::io`).

## Complexity

Linear in the size of the directory's files.

## Exceptions

- (1) What the reads and writes of the files throw; their errors are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> pack() {
    auto done = co_await compress::tar::async_create("site", "site.tgz");
    println("{}", done.has_value());
}

int main() {
    (void)io::mkdir_all("site/css");
    (void)io::write_file("site/index.html", "<h1>hello</h1>\n");
    (void)io::write_file("site/css/site.css", "h1 { color: teal }\n");
    async::spawn(pack()).wait();

    io::reader file = *io::open("site.tgz");
    compress::gzip::reader unpacked(file);
    compress::tar::reader r(unpacked);
    while (auto e = r.next()) {
        if (!*e) {
            break;
        }
        println("{}", (*e)->name);
    }
    (void)r.close();
    (void)io::remove_all("site");
    (void)io::remove("site.tgz");
}
```

Output:

```text
true
css/
css/site.css
index.html
```

## See also

- [extract](tar-extract.md): the other way
- [writer](tar-writer.md): an archive entry by entry
- [sgcl::compress::tar](tar.md)
