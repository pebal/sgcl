[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md)

# sgcl::compress::tar::reader

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    class reader;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::tar::reader` reads a tar archive entry by entry from another reader, `in`, as Go's `tar.Reader`:
[next](next.md) goes to the next entry's header, past whatever of the current one's data was not read,
and the reads give that entry's data, exactly its size, then 0. It is an [io reader](../../io/reader/README.md) of the current
entry, so the stream functions of the mixin (`read_all`, `copy_to`) read one entry.

The archive may be v7, ustar, pax (an entry's extended header and the global ones, which apply to every entry after
them), GNU (long names and links, base-256 numbers, the access and change times) or star. Numbers in octal and in
base-256 are read, a header's checksum both unsigned and signed (as old Sun tar wrote it). The archive ends, as in
Go, at two blocks of zeros, at one followed by the end of the input, or at the end of the input where a header would
start; a block of zeros followed by anything else is `errc::invalid_header`.

## Rules

- The reader is move-only, and read by one thread or task at a time.
- A reader moved from has no stream, it went with the move: [next](next.md) gives `errc::io` with
  `io::errc::closed`, a read gives `io::errc::closed`, and its [close](close.md) closes nothing. A reader
  moved onto itself is unchanged.
- A pax header or a GNU long name larger than 1 MiB is `errc::too_large`, and the global records altogether no
  more; a size that is negative or not a number is `errc::invalid_header`; data that ends inside an entry is
  `errc::unexpected_end`, naming the entry. These stop the reader: every call after gives the same error. A sparse
  file (GNU's `'S'`, a pax `GNU.sparse` record) and a GNU multi-volume part are `errc::unsupported`, which stops only
  that entry: `next` goes on to the one after it.
- It reads its input 64 KB at a time (32 KB in the managed block of a task's reads), so it may take bytes past the
  end of the archive from its source.
- [close](close.md) closes `in`, as `io::buffered_reader`'s close does.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](tar-reader.md) | a reader of the archive `in` holds |
| `(destructor)` | destroys the reader; `in` is not closed |

#### Operations

| Function | Description |
|---|---|
| [next, async_next](next.md) | the next entry, `nullopt` at the end of the archive |
| [read, async_read](read.md) | the current entry's data |
| [close, async_close](close.md) | closes `in` |

#### Observers

| Function | Description |
|---|---|
| [last_error](last_error.md) | the whole of the last failure, kept |

#### From mixin::reader

| Function | Description |
|---|---|
| [read_full, async_read_full](../../io/mixin/reader/read_full.md) | fills the whole buffer from the entry's data |
| [read_all, async_read_all](../../io/mixin/reader/read_all.md) | the entry's data, as bytes |
| [read_all_text, async_read_all_text](../../io/mixin/reader/read_all_text.md) | the entry's data, as text |
| [copy_to, async_copy_to](../../io/mixin/reader/copy_to.md) | the entry's data, written to a writer |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("site/css");
    (void)io::write_file("site/index.html", "<h1>hello</h1>\n");
    (void)io::write_file("site/css/site.css", "h1 { color: teal }\n");
    (void)compress::tar::create("site", "site.tar.gz");

    io::reader file = *io::open("site.tar.gz");
    compress::gzip::reader unzipped(file);
    compress::tar::reader r(unzipped);
    while (auto e = r.next()) {
        if (!*e) {
            break;  // the end
        }
        if ((*e)->type == compress::tar::kind::file && (*e)->is_local()) {
            println("{}: {}", (*e)->name, r.read_all()->size());
        }
    }
    (void)r.close();  // and with it the gzip reader's file
    (void)io::remove_all("site");
    (void)io::remove("site.tar.gz");
}
```

Output:

```text
css/site.css: 19
index.html: 15
```

## See also

- [writer](../tar-writer/README.md): the other way
- [extract](../tar-extract.md): the whole archive into a directory
- [sgcl::compress::tar](../tar.md)
