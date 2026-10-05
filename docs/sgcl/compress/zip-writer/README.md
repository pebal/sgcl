[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md)

# sgcl::compress::zip::writer

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::zip {
    class writer;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::compress::zip::writer` writes a zip archive entry after entry into another writer, `out`, as Go's
`zip.Writer`: [create](create.md) gives an [io writer](../../io/writer/README.md) of an entry's data, which the next
`create` ends (the entry writer's `close()` does too, and may be left out); [add](add.md) writes a whole
entry; [close](close.md) ends the last entry and writes the central directory, with the ZIP64 records
when there are 65 535 entries or more or an offset past 4 GiB, and leaves `out` open. The local headers carry no
sizes (a data descriptor after the data has them), so `out` is written straight through and never needs to seek: a
socket, an HTTP response.

## Rules

- The writer holds `out` and a state shared with its entry writers. It is moved, not copied: a copy would share the
  archive's state and not its current entry. One thread or task writes it at a time.
- A writer moved from is closed, with no archive: its [create](create.md), [add](add.md) and
  [set_comment](set_comment.md) are refused as after the close (kept as its first error), and its
  [close](close.md) does nothing. A writer moved onto itself is unchanged.
- Writing to an entry that was ended is `io::errc::closed`, that write's own error: nothing is written and nothing
  kept, and the archive goes on.
- Every other error the writer gives is kept as its first: a failure of `out`, and the caller's too — data for a
  directory, an entry `create` refuses (a name too long, a method not written), a create after the close, a comment
  too long or set after the close. Every write, `add`, `set_comment` and `close` after it gives that error at
  once and writes nothing, and `create` then gives an entry writer whose writes give it. So an archive is written
  freely and checked once, at the close; [last_error](last_error.md) holds the error. None of these
  errors comes from data read, so none has a place: the message is the words alone.
- A task's writes (`async_create`, `async_add`, `async_close`) hand out the headers, the descriptors and the
  directory from a managed block the writer keeps; a task's `create` writes its entry's local header with the
  entry's first bytes, in one write, whose failure is kept as any other.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](zip-writer.md) | a writer of an archive into `out` |
| `(destructor)` | destroys the writer; the archive is not closed |

#### Operations

| Function | Description |
|---|---|
| [create, async_create](create.md) | starts an entry: an io writer of its data |
| [add, async_add](add.md) | a whole entry |
| [add_file](add_file.md) | a file as a whole entry, with its mode and time |
| [set_comment](set_comment.md) | the archive's comment |
| [close, async_close](close.md) | the last entry ended, the central directory written; `out` stays open |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | checks whether the archive was closed |
| [last_error](last_error.md) | the first error the writer gave, kept |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::writer file = *io::create("backup.zip");
    compress::zip::writer w(file);
    (void)w.add("notes.txt", "remember the milk\n");  // a text is its bytes, a vector<byte> too
    io::writer log = *w.create("logs/app.log");
    for (string line : {"started\n", "stopped\n"}) {
        (void)log.write(line);
    }
    if (auto done = w.close(); !done) {  // a failure of any step above
        println("{}", done.error().message());
    }
    (void)file.close();

    auto a = compress::zip::archive::open("backup.zip");
    for (auto& e : a->entries()) {
        println("{}: {} bytes", e.name, e.size);
    }
    (void)a->close();
    (void)io::remove("backup.zip");
}
```

Output:

```text
notes.txt: 18 bytes
logs/app.log: 16 bytes
```

## See also

- [archive](../zip-archive/README.md): the other way
- [create](../zip-create.md): a whole directory into an archive
- [sgcl::compress::zip](../zip.md)
