[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md)

# sgcl::compress::sevenzip::archive

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    class archive;
}
```

`sgcl::compress::sevenzip::archive` is a 7z archive to read: its header read when it is opened, the entries then open in
any order and any number of times at once, or all of them in the archive's order. [open](open.md) reads
the signature header and the header at the end — decoding it first when it is packed, as 7-Zip packs it by default — and
nothing else; [from](from.md) does the same for an archive in memory. An entry's data is read when it
is asked for.

In a solid archive (7-Zip's default) an entry's data lies inside a folder after the data of the entries before it:
[reader](reader.md) and [read](read.md) decode the entry's folder from its start
and drop what comes before the entry, so readers of any entries may be open at once, from any threads, but reading
every entry that way costs the square of a folder's size. [walk](walk.md) reads them all in the
archive's order with one decoder a folder.

## Rules

- A value, copied cheaply: the entries and the source are shared by the copies. It lives where a `tracked_ptr` may:
  a stack, a task's frame, a managed object.
- What the archive checks and what it refuses before anything is allocated are the format's
  ([sevenzip](../sevenzip.md#rules)); the bounds are the [limits](../limits.md) given to `open`, `from` or a read.
- **No password:** an encrypted entry is listed, with `encrypted`, and reading it is `errc::password_required`
  (`"7z: password required"`); when the header is encrypted too (7-Zip's `-mhe=on`), the names are hidden and
  opening the archive is `errc::password_required`.
- **A wrong password** is `errc::wrong_password` (`"7z: wrong password"`), the same error from `open` (an encrypted
  header), `read`, `reader` and `walk`. 7zAES has no check of the key: a wrong key decrypts to noise, which the
  decoders or the CRC-32 reject — and damaged encrypted data decrypts to noise just the same, so a damaged encrypted
  archive is a wrong password too (7-Zip says "Wrong password?" for both). Only failures of the data are said so; a
  folder whose coders cannot be made (unknown methods, damaged properties) says that.
- **The rounds.** 7-Zip writes k = 19 (524 288 rounds, about 12 ms here); reading takes k up to 24 (about 0.4 s), and
  a larger k is `errc::too_large`, before any round is run — a header asking for 2^40 rounds would not finish.
  k = 63, the key the salt and the password themselves with no hashing, is read.
- **Deflate64** (`-m0=Deflate64`): DEFLATE with a window of 64 KB, lengths up to 65 538 and two more distance codes,
  decoded by the same decoder as Deflate, its window twice 64 KB.
- [close](close.md) closes the file the archive opened itself from a path; a file given to `open` is
  the caller's.
- **Tasks.** `async_open` reads the headers (and a packed header's streams) into managed memory the slices given to
  the file hold, then decodes from memory. A task's read of an entry of an archive in a file runs the decoding on the
  [blocking pool](../../async/spawn_blocking.md), where the file is read: the job holds the entry's reader (a managed
  object, whose decoders' plain memory it owns) and the slice of the caller's buffer, so a task let go of meanwhile
  leaves the job nothing freed. An archive in memory is decoded on the worker, which it lets go every 64 KB.

## Member functions

| Function | Description |
|---|---|
| [open, async_open](open.md) | an archive of a file, its header read (static) |
| [from](from.md) | an archive in memory (static) |
| [close](close.md) | closes the file the archive opened itself |

#### Lookup

| Function | Description |
|---|---|
| [entries](entries.md) | every entry, in the archive's order |
| [find](find.md) | the first entry of a name |

#### Operations

| Function | Description |
|---|---|
| [reader](reader.md) | an io reader of an entry's data, its folder decoded from the start |
| [read, async_read](read.md) | an entry's data whole, checked against the limits |
| [walk, async_walk](walk.md) | every entry in order with a reader of its data, one decoder a folder |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::sevenzip::writer w("backup.7z", {.level = 9});
    w.add("notes.txt", "remember the milk\n");  // a text is its bytes
    w.add_directory("logs");
    io::writer log = w.create("logs/app.log");
    for (int i : range(3)) {
        (void)log.write("a line of the log\n");  // written freely...
    }
    if (auto done = w.close(); !done) {  // ...and checked once, here
        println("{}", done.error().message());
        return 1;
    }
    compress::sevenzip::archive a = *compress::sevenzip::archive::open("backup.7z");
    for (auto [e, r] : a.walk()) {
        string text = e.is_directory ? string("(directory)\n")
                                     : r.read_all_text().value_or(string("?"));
        print("{}: {}", e.name, text);
    }
    (void)a.close();
    (void)io::remove("backup.7z");
}
```

Output:

```text
notes.txt: remember the milk
logs: (directory)
logs/app.log: a line of the log
a line of the log
a line of the log
```

## See also

- [writer](../sevenzip-writer/README.md): the other way
- [extract](../sevenzip-extract.md): the whole archive into a directory
- [sgcl::compress::sevenzip](../sevenzip.md)
