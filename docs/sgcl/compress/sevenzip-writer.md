[sgcl](../README.md) › [compress](README.md) › [sevenzip](sevenzip.md)

# sgcl::compress::sevenzip::writer

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    class writer;
}
```

`sgcl::compress::sevenzip::writer` writes a 7z archive entry after entry, as 7-Zip writes it: the data of entries
into folders (solid by default, a new folder at a new filter or past the block), encoded as it comes and never held
whole. [create](sevenzip-writer/create.md) gives an [io writer](../io/writer.md) of an entry's data; the next
`create`, `add` or `add_directory` ends the entry (its `close()` does too, and may be left out), and writing to an
entry that was ended is `io::errc::closed`, that write's own error. An entry with no data is an empty file; a directory has none. The data
goes through the folder's coders as it is written, in plain memory the writer keeps from folder to folder, and out a
quarter of a megabyte at a time. [close](sevenzip-writer/close.md) ends the last entry and folder and writes the
header, packed with LZMA as 7-Zip packs it, and then the signature header at the archive's start — which is why a 7z
archive needs an output that seeks back: a file (its start rewritten with `pwrite`), or an
[io::buffer](../io/buffer.md) (which seeks as a file does). A socket cannot take one.

## Rules

- The writer holds its state as a tracked word, so it lives where a `tracked_ptr` may. It is moved, not copied, and
  written by one thread or task at a time.
- A writer moved from is closed, with no archive: its [create](sevenzip-writer/create.md),
  [add](sevenzip-writer/add.md) and [add_directory](sevenzip-writer/add_directory.md) are refused as after the close
  (kept as its first error), and its [close](sevenzip-writer/close.md) does nothing. A writer moved onto itself is
  unchanged.
- Entries are written in the order they come; 7-Zip also sorts them by extension first, which a program does itself
  by adding them in that order. How the folders are cut, the filters chosen and the levels read are the
  [options](sevenzip-options.md)'.
- Names are written in UTF-16LE (given in UTF-8; a trailing `/` dropped; an empty name, a NUL or bytes that are not
  UTF-8 refused), with the times and the attributes of the [entry_info](sevenzip-entry_info.md).
- Every error the writer gives but a write to an entry that was ended (which fails by itself, nothing kept) is kept
  as its first: a failure of the output (a file that cannot be made at the path included), and the caller's too — a
  name it refuses, data for a directory, an entry after the close. Every write, `add`, `add_directory` and `close` after it gives that error at once and writes
  nothing, and `create` then gives an entry writer whose writes give it. So an archive is written freely and checked
  once, at the close; [last_error](sevenzip-writer/last_error.md) holds the error. None of these errors comes from
  data read, so none has a place: the message is the words alone.
- The encoder's memory (for LZMA2 at level 6 about 90 MiB, at 9 about 700 MiB, as xz's) is taken at the first folder
  and given back at `close`, not when the collector takes the writer.
- With a password, the key is made once, when the writer is made (about 12 ms), and kept in plain memory the writer
  zeroes at `close`; the writer keeps no copy of the password.
- **Tasks.** An entry writer's `async_write` encodes in portions of 64 KB and lets the worker go between them, and
  writes the output through a managed block the file's write holds; `async_close` does the last folder and the
  header the same way.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](sevenzip-writer/sevenzip-writer.md) | a writer into a file made at a path, an open file, or a buffer |
| `(destructor)` | destroys the writer; the archive is not closed |

#### Operations

| Function | Description |
|---|---|
| [create](sevenzip-writer/create.md) | starts an entry: an io writer of its data |
| [add](sevenzip-writer/add.md) | a whole entry |
| [add_file](sevenzip-writer/add_file.md) | a file as a whole entry, with its mode and time |
| [add_directory](sevenzip-writer/add_directory.md) | a directory |
| [close, async_close](sevenzip-writer/close.md) | the last folder ended, the header and the signature header written |

#### Observers

| Function | Description |
|---|---|
| [is_closed](sevenzip-writer/is_closed.md) | checks whether the archive was closed |
| [last_error](sevenzip-writer/last_error.md) | the first error the writer gave, kept |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::sevenzip::writer w("backup.7z", {.level = 9});
    w.add("notes.txt", "remember the milk\n");
    w.add_directory("logs");
    io::writer log = w.create("logs/app.log");
    (void)log.write("started\n");
    (void)log.write("stopped\n");
    if (auto done = w.close(); !done) {
        println("{}", done.error().message());
        return 1;
    }
    auto a = compress::sevenzip::archive::open("backup.7z");
    for (auto& e : a->entries()) {
        println("{}: {} bytes", e.name, e.size);
    }
    (void)a->close();
    (void)io::remove("backup.7z");
}
```

Output:

```text
notes.txt: 18 bytes
logs: 0 bytes
logs/app.log: 16 bytes
```

## See also

- [archive](sevenzip-archive.md): the other way
- [create](sevenzip-create.md): a whole directory into an archive
- [sgcl::compress::sevenzip](sevenzip.md)
