[sgcl](../README.md) › [compress](README.md) › [sevenzip](sevenzip.md)

# sgcl::compress::sevenzip::options

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    struct options {
        sevenzip::method method = sevenzip::method::lzma2;
        compress::level level;
        bool solid = true;
        uint64_t solid_block = 0;
        bool auto_filters = true;
        optional<slice<const byte>> password;
        bool encrypt_header = true;
        limits limit;
    };
}
```

`sgcl::compress::sevenzip::options` is how an archive is written, and what reading one needs: the password and the
limits, which are all a reader takes (`archive::open`, `from`, [extract](sevenzip-extract.md)); the
[writer](sevenzip-writer.md) and [create](sevenzip-create.md) take the rest.

## Rules

- An aggregate: `{.level = 9, .solid = false}`, `{.password = secret}`, the other fields at their defaults.
- **The password** is the caller's bytes — a [crypto::secret_bytes](../crypto/secret.md), a buffer or a literal —
  pointed to and never copied into managed memory. They are read when the call is made (the archive opened, the
  writer made, the extract or the create started), turned then into the key derivation's UTF-16LE in plain memory
  and zeroed when the archive or the writer goes; so they have to live until that call and not after. The `async_`
  forms read them before the task is made. A key is made once for each salt and k and kept with the archive (7-Zip's
  archives and these use one salt for every folder: one derivation an archive), in a
  [crypto::secret](../crypto/secret.md).
- **Folders.** In a solid archive (the default) entries go one after another into a folder until it has taken
  `solid_block` bytes — by default 128 times the dictionary for LZMA and LZMA2, 16 times the model's memory for
  PPMd, within 16 MiB to 4 GiB, and 16 MiB for Deflate and Copy, as 7-Zip reckons it — or until an entry calls for
  another filter; `solid = false` gives every entry a folder of its own, which starts the encoder over for every
  entry and empties its hash table each time: tens of thousands of small entries are written much faster solid.
- **Automatic filters** (`auto_filters`, before every coder but Copy, as 7-Zip puts them): an entry's first 4 KB
  decide. A program — ELF, Mach-O or PE — gets the branch converter of the processor its header names: x86 and
  x86-64 BCJ, ARM, ARM-Thumb (PE's Thumb machines), ARM64, RISC-V, PowerPC and SPARC (big-endian ELF, Mach-O
  PowerPC), IA-64; a WAV file of PCM samples gets Delta by its block align (4 for 16-bit stereo). The name does not
  decide (a `.exe` that is no PE gets nothing), a universal Mach-O (two processors in one file) gets nothing, links
  and directories get nothing.
- **Writing with a password:** every folder is encrypted with 7zAES — k = 19 as 7-Zip writes it, a random salt of
  16 bytes for the archive and a random IV of 16 bytes for each folder, from [crypto::random](../crypto/random.md) —
  and with `encrypt_header` (the default, as `-mhe=on`) the header too, so that the names cannot be read without the
  password; `encrypt_header = false` leaves the header plain, the names listed by anyone. Entries without data
  (directories, empty files) are in no folder: only an encrypted header hides them.

## Member objects

| Member | Description |
|---|---|
| `method` | the coder of the folders ([method](sevenzip-method.md)); `method::lzma2` by default |
| `level` | 0 to 9, 6 by default; read by the method ([level](level.md)) |
| `solid` | entries one after another in a folder; `true` by default |
| `solid_block` | the data a folder takes before a new one starts; 0, by default, is 7-Zip's for the settings |
| `auto_filters` | a branch converter or Delta by what an entry's first bytes are; `true` by default |
| `password` | 7zAES, UTF-8: to read encrypted entries and headers; to write, every folder encrypted; none by default |
| `encrypt_header` | with a password, the header too, the names hidden (7-Zip's `-mhe=on`); `true` by default |
| `limit` | reading: `max_size`, `max_memory`, `max_entries` ([limits](limits.md)) |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string text = string("the same words again and again. ").repeat(100);
    compress::sevenzip::entry_info info;  // a fixed time: the same bytes every run
    info.modified = time::datetime::from_unix(1790000000, time::zone::utc());
    for (bool solid : {true, false}) {
        io::buffer archive;
        compress::sevenzip::writer w(archive, {.level = 9, .solid = solid});
        for (string name : {"a.txt", "b.txt", "c.txt"}) {
            w.add(name, text, info);
        }
        (void)w.close();
        println("solid {}: {} bytes", solid, archive.size());
    }
}
```

Output:

```text
solid true: 231 bytes
solid false: 327 bytes
```

## See also

- [method](sevenzip-method.md), [entry_info](sevenzip-entry_info.md)
- [limits](limits.md)
- [sgcl::compress::sevenzip](sevenzip.md)
