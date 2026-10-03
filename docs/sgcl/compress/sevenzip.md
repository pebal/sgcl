[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::sevenzip

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress.h"

namespace sgcl::compress::sevenzip {
    using error = compress::error;
    struct entry;
    enum class method : uint8_t;
    struct options;
    struct entry_info;
    class archive;
    class writer;

    expected<void, error> extract(const string& archive_path, const string& directory,
                                  const options& o = {});
    async::task<expected<void, error>> async_extract(string archive_path, string directory,
                                                     options o = {}) noexcept;
    expected<void, error> create(const string& directory, const string& archive_path,
                                 const options& o = {});
    async::task<expected<void, error>> async_create(string directory, string archive_path,
                                                    options o = {}) noexcept;
}
```

`sgcl::compress::sevenzip` is 7z, the format of 7-Zip (`.7z`). Reading takes every method 7-Zip and libarchive
write — LZMA, LZMA2, PPMd, BZip2, Deflate, Deflate64, Copy — with the filters before them: the branch converters of
x86 (BCJ and BCJ2), ARM, ARM-Thumb, ARM64, PowerPC, SPARC, IA-64 and RISC-V, and Delta; solid and non-solid archives,
headers plain and packed, empty files, directories, symbolic links, anti-items, times and attributes, and passwords
(7zAES), the header encrypted too or not. Writing makes what 7-Zip makes: LZMA2 (or LZMA, PPMd, Deflate, Copy),
solid, the filters chosen by what each file is, the header packed with LZMA, and with a password everything
encrypted; Deflate64 is read, not written.

Every archive 7-Zip 26 writes with these methods reads here, and 7-Zip reads every archive written here; libarchive
3.7 (bsdtar) reads only some of 7-Zip's — large PPMd, BZip2 and Deflate archives it calls truncated or damaged, and
none with a password — so the reference for what an archive holds is 7-Zip. The namespace has an
[archive](sevenzip-archive.md) to read, its entries in any order or all of them in order, a
[writer](sevenzip-writer.md) that writes entry after entry, and what `7zz x` and `7zz a` do to a directory
([extract](sevenzip-extract.md), [create](sevenzip-create.md)). With a password,
`compress::sevenzip::archive::open("backup.7z", {.password = secret})` reads an archive and
`compress::sevenzip::extract("backup.7z", "restored", {.password = secret})` unpacks one.

## Rules

- **Solid archives.** 7-Zip puts files one after another into a *folder* and compresses the folder as one stream (a
  *solid* archive, the default), so an entry's data lies after the data of the entries before it in its folder.
  Reading one entry decodes its folder from the start; [walk](sevenzip-archive/walk.md) reads them all in the
  archive's order with one decoder a folder.
- **Checked.** The signature header, the header and a packed header have CRC-32s; every entry's data is counted to
  its size and checked against its CRC-32, and a folder with a CRC of its own is checked at its end
  (`errc::checksum`). Every coder of a folder must end where the header says its output ends — LZMA's and PPMd's
  range coders at zero, LZMA2's end byte, Deflate's last block, BZip2's end of stream, every stream of BCJ2 used up —
  as 7-Zip checks it, so a damaged stream that decodes to the right bytes and goes on is `errc::corrupt`.
- **Refused before anything is allocated:** a count in the header (files, folders, coders, bind pairs, streams)
  larger than the bytes of the header can hold, more entries than the [limits](limits.md)' `max_entries`
  (1 000 000), a header past 64 MiB (packed or not), a folder whose decoders need more than `max_memory` (1 GiB;
  LZMA's and LZMA2's dictionaries are counted as large as the folder's output when that is smaller, PPMd's memory as
  its properties say), and in a whole read an entry larger than `max_size`.
- **Passwords** are the [options](sevenzip-options.md)' `password`: the caller's bytes, read when the archive is
  opened or the writer made. 7zAES is AES-256 in CBC mode, its key made from the password — given in UTF-8, hashed in
  UTF-16LE as 7-Zip hashes it — by 2^k rounds of SHA-256 over a salt, the password and the round's number. Without a
  password an encrypted entry is listed, with `encrypted`, and reading it is `errc::password_required`; when the
  header is encrypted too (7-Zip's `-mhe=on`), opening the archive is. A wrong password is `errc::wrong_password`,
  from `open` (an encrypted header), `read`, `reader` and `walk` alike ([sevenzip::archive](sevenzip-archive.md)).
- Methods the library does not know (Rar, ARJ, 7-Zip's own external codecs) are `errc::unsupported`, named in the
  error with their ID.
- The writer keeps its first error, as every stream of the module does: an archive is written freely and checked
  once, at the close.

## Member types

| Type | Definition |
|---|---|
| `error` | [compress::error](error.md) |
| [entry](sevenzip-entry.md) | one entry: its name, size, times, attributes, CRC-32, where its data lies |
| [method](sevenzip-method.md) | the coder of a written archive's folders |
| [options](sevenzip-options.md) | how an archive is written; the password and the limits of reading |
| [entry_info](sevenzip-entry_info.md) | what a written entry is besides its name and data |
| [archive](sevenzip-archive.md) | an archive to read: its entries in any order, or all of them in order |
| [writer](sevenzip-writer.md) | an archive written entry after entry |

## Member functions

| Function | Description |
|---|---|
| [extract, async_extract](sevenzip-extract.md) | an archive unpacked into a directory, every name checked first |
| [create, async_create](sevenzip-create.md) | a directory packed into an archive |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    {
        compress::sevenzip::writer w("secret.7z", {.password = "correct horse"});
        w.add("plan.txt", "meet at noon\n");
        if (auto done = w.close(); !done) {
            println("{}", done.error().message());
            return 1;
        }
    }
    (void)compress::sevenzip::extract("secret.7z", "restored", {.password = "correct horse"});
    print("{}", io::read_text("restored/plan.txt").value_or(string("?")));
    (void)io::remove_all("restored");
    (void)io::remove("secret.7z");
}
```

Output:

```text
meet at noon
```

## See also

- [xz](xz.md), [lzma](lzma.md): the methods; [bzip2](bzip2.md), [flate](flate.md): the others read
- [zip](zip.md), [tar](tar.md): the other archives
- [crypto::secret](../crypto/secret.md): a password kept in memory the collector does not copy
- `tests/compress/sevenzip.cpp`: archives of every method made by 7-Zip and libarchive, read and compared with the
  files; passwords both ways, AES-256-CBC on NIST SP 800-38A, the key against one computed apart;
  `tests/compress/files.cpp`: `extract` and `create` both ways against 7-Zip's `7zz`;
  `tests/compress/fuzz/sevenzip_fuzz.cpp`, `sevenzip_writer_fuzz.cpp`, `sevenzip_aes_fuzz.cpp`
- [sgcl::compress](README.md)
