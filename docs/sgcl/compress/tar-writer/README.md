[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md)

# sgcl::compress::tar::writer

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress.h"

namespace sgcl::compress::tar {
    class writer;
}
```

`sgcl::compress::tar::writer` writes a tar archive entry by entry into another writer, `out`, as Go's `tar.Writer`:
[write_header](write_header.md), then exactly the entry's size in bytes through
[write](write.md), and [close](close.md) the two blocks of zeros that end the archive, `out`
left open. It is an [io writer](../../io/writer/README.md) of the current entry's data, so `copy_from` and the text forms of
`write` write into the entry.

A header is ustar when the entry fits in one; otherwise a pax extended header goes before it, with a record for each
field that does not: a name past ustar's (100 bytes, or a prefix of 155 and a name of 100 split at a slash) or not
ASCII, a link name likewise, a user or group name past 32 bytes or not ASCII, an id past 2 097 151 or negative, a
size of 8 GiB or more, a time with a fraction of a second or outside eleven octal digits (1970 to 2242), an access or
change time, and the entry's own records (`entry::pax`). With an extended header a name past 100 bytes goes into it
rather than into the prefix, and the records are in the order of their keys, as Go writes them: the same entries make
Go's bytes, and the golden archives of Go's tests are matched byte for byte (the one known difference is the name of
a pax header written for a directory, where Go keeps the directory's trailing part).

## Rules

- The writer holds `out`, a handle of a tracked word, so it lives where a `tracked_ptr` may: a stack, a task's
  frame, a managed object. It is move-only, and written by one thread or task at a time.
- A writer moved from is closed, its output gone with the move: its [close](close.md) does nothing, a
  [write_header](write_header.md) gives `errc::io` with `io::errc::closed` and a write gives
  `io::errc::closed`, kept as its first error. A writer moved onto itself is unchanged.
- A header before the last entry's data is whole, or a close, is `errc::invalid_argument`, and so is an entry the
  format cannot hold: an empty name, a NUL in a name, a size for a type that has no data, a device number past
  2 097 151, a pax record that is malformed, given twice or one of the fields'. Data past the entry's size is
  `errc::invalid_argument`, and nothing of that write is written.
- Every error the writer gives is kept as its first: a failure of `out`, and the caller's too (a header or a write
  it cannot take, a call after the close). Every `write_header`, `write` and `close` after it gives that error at
  once and writes nothing, so an archive is written freely and checked once, at the close;
  [last_error](last_error.md) holds it. None of these errors comes from data read, so none has a place:
  the message is the words alone.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](tar-writer.md) | a writer of an archive into `out` |
| `(destructor)` | destroys the writer; the archive is not closed |

#### Operations

| Function | Description |
|---|---|
| [write_header, async_write_header](write_header.md) | starts an entry: its header |
| [write, async_write](write.md) | the current entry's data |
| [add_file](add_file.md) | a file as a whole entry, its header and its data, with its mode and time |
| [close, async_close](close.md) | the last padding and two blocks of zeros; `out` stays open |

#### Observers

| Function | Description |
|---|---|
| [last_error](last_error.md) | the first error the writer gave, kept |

#### From mixin::writer

| Function | Description |
|---|---|
| [write, async_write](../../io/mixin/writer/write.md) | writes a string, a text slice, a literal, a C string, a `std::string_view` or one byte |
| [copy_from, async_copy_from](../../io/mixin/writer/copy_from.md) | writes everything a reader gives, to its end |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::writer file = *io::create("notes.tar.gz");
    compress::gzip::writer packed(file);
    compress::tar::writer w(packed);
    for (string name : {"monday.txt", "tuesday.txt"}) {
        string text = "notes of " + name + "\n";
        (void)w.write_header({.name = name, .size = text.size(), .mode = io::permissions(0644)});
        (void)w.write(text);
    }
    if (auto done = w.close(); !done) {  // the first error of any call, kept
        println("{}", done.error().message());
    }
    (void)packed.close();
    (void)file.close();

    io::reader in = *io::open("notes.tar.gz");
    compress::gzip::reader unpacked(in);
    compress::tar::reader r(unpacked);
    while (auto e = r.next()) {
        if (!*e) {
            break;
        }
        print("{}: {}", (*e)->name, r.read_all_text().value_or(string()));
    }
    (void)r.close();
    (void)io::remove("notes.tar.gz");
}
```

Output:

```text
monday.txt: notes of monday.txt
tuesday.txt: notes of tuesday.txt
```

## See also

- [reader](../tar-reader/README.md): the other way
- [create](../tar-create.md): a whole directory into an archive
- [sgcl::compress::tar](../tar.md)
