[sgcl](../README.md) › [compress](README.md) › [zip](zip.md)

# sgcl::compress::zip::create, async_create

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    expected<void, error> create(const string& directory, const string& archive_path,         // (1)
                                 const options& o = {});
    async::task<expected<void, error>> async_create(string directory, string archive_path,    // (2)
                                                    options o = {}) noexcept;
}
```

Packs the directory into an archive, as `zip -r` does: `zip::create("site", "site.zip")`, the files deflated or
stored by the options' method. The entries are named from the directory, not with it (`index.html`, `css/`,
`css/site.css`), as Go's `AddFS` names them, in lexical order, with their mode and time; a symbolic link is archived
as a link; a socket, a device or a fifo is left out. A failure removes the half-made archive.

1. Blocks the calling thread.
2. Returns a task that runs the work on the [blocking pool](../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `directory` | the directory packed |
| `archive_path` | the archive written |
| `o` | `method`, store or deflate ([options](zip-options.md)) |

## Return value

Nothing, or the [error](error.md): `method::deflate64` (`errc::unsupported`), what the [writer](zip-writer.md)
refuses, a failure of the file system (`errc::io`).

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
    auto done = co_await compress::zip::async_create("site", "site.zip");
    println("{}", done.has_value());
}

int main() {
    (void)io::mkdir_all("site/css");
    (void)io::write_file("site/index.html", "<h1>hello</h1>\n");
    (void)io::write_file("site/css/site.css", "h1 { color: teal }\n");
    async::spawn(pack()).wait();

    auto a = compress::zip::archive::open("site.zip");
    for (auto& e : a->entries()) {
        println("{}", e.name);
    }
    (void)a->close();
    (void)io::remove_all("site");
    (void)io::remove("site.zip");
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

- [extract](zip-extract.md): the other way
- [writer](zip-writer.md): an archive entry by entry
- [sgcl::compress::zip](zip.md)
