# sgcl::compress::sevenzip

```cpp
#include "sgcl/compress/sevenzip.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress::sevenzip {
    struct entry;
    class archive;   // an archive to read: its entries in any order, or all of them in order
    enum class method : uint8_t { lzma2, lzma, ppmd, deflate, copy };
    struct options;  // how an archive is written; a password and the limits for reading
    struct entry_info;
    class writer;    // an archive written entry after entry

    expected<void, error> extract(const string& archive_path, const string& directory, const options& o = {});
    async::task<expected<void, error>> async_extract(string archive_path, string directory, options o = {});
    expected<void, error> create(const string& directory, const string& archive_path, const options& o = {});
    async::task<expected<void, error>> async_create(string directory, string archive_path, options o = {});
}
```

7z, the format of 7-Zip (`.7z`). Reading takes every method 7-Zip and libarchive write — LZMA, LZMA2, PPMd, BZip2, Deflate, Deflate64, Copy — with the filters before them: the branch converters of x86 (BCJ and BCJ2), ARM, ARM-Thumb, ARM64, PowerPC, SPARC, IA-64 and RISC-V, and Delta; solid and non-solid archives, headers plain and packed, empty files, directories, symbolic links, anti-items, times and attributes, and passwords (7zAES), the header encrypted too or not. Writing makes what 7-Zip makes: LZMA2 (or LZMA, PPMd, Deflate, Copy), solid, the filters chosen by what each file is, the header packed with LZMA, and with a password everything encrypted; Deflate64 is read, not written. Every archive 7-Zip 26 writes with these methods reads here, and 7-Zip reads every archive written here; libarchive 3.7 (bsdtar) reads only some of 7-Zip's — large PPMd, BZip2 and Deflate archives it calls truncated or damaged, and none with a password — so the reference for what an archive holds is 7-Zip.
With a password, `compress::sevenzip::archive::open("backup.7z", {.password = secret})` reads an archive and `compress::sevenzip::extract("backup.7z", "restored", {.password = secret})` unpacks one.

## Members

### entry

```cpp
struct entry {
    string name;                                // UTF-8, '/' between the parts
    uint64_t size = 0;                          // decompressed
    bool is_directory = false;
    bool is_anti = false;                       // an update's mark that the name was deleted
    optional<uint32_t> crc;                     // of the data; none for entries without data
    optional<time::datetime> modified, created, accessed;
    uint32_t attributes = 0;                    // Windows' low 16 bits; with 0x8000, POSIX st_mode in the high 16
    bool encrypted = false;                     // its folder is encrypted (7zAES): read with a password
    uint32_t folder = UINT32_MAX;               // where its data lies: its folder (none without data)
    uint64_t offset = 0;                        // and where in the folder's output it starts
    bool is_symlink() const noexcept;           // POSIX attributes naming a link: the data is the target
    bool is_local() const noexcept;
};
```

What the archive's header says of the entry. Names are UTF-16LE in the archive and UTF-8 here; a surrogate without its pair is `errc::corrupt` when the archive is opened. The times are the header's FILETIMEs, in UTC. An entry without data is a directory unless the header marks it an empty file; an anti-item (what `7zz u` with `-u…q3` writes into an archive of differences) is an entry with `is_anti`, directory or file, and no data. **`is_local()`** is the rule of [zip](zip.md) and [tar](tar.md): not empty, not absolute, no NUL, no `\` (7z's names use `/`), no `..` that climbs above the start — the check a program makes before joining a name to a directory.

### archive

```cpp
static expected<archive, error> open(const string& path);                     // and (path, limits), (path, options); + async_open
static expected<archive, error> open(const io::file& file);                   // and (file, limits), (file, options); + async_open
static expected<archive, error> from(const slice<const byte>& data);          // and (data, limits), (data, options): an archive in memory
slice<const entry> entries() const noexcept;
optional<entry> find(const string& name) const;                               // the first of that name
expected<io::reader, error> reader(const entry& e) const;                     // and (e, limits), (name)
expected<vector<byte>, error> read(const entry& e) const;                     // and (e, limits), (name), (name, limits); + async_read
generator<pair<entry, io::reader>> walk() const;                              // and (limits)
async::generator<pair<entry, io::reader>> async_walk() const;                 // and (limits)
expected<void, error> close();
```

Opening reads the signature header and the header at the end — decoding it first when it is packed, as 7-Zip packs it by default — and nothing else. An entry's data is read when it is asked for.

- **Solid archives.** 7-Zip puts files one after another into a *folder* and compresses the folder as one stream (a *solid* archive, the default), so an entry's data lies after the data of the entries before it in its folder. `reader(e)` and `read(e)` decode the entry's folder from its start and drop what comes before the entry: readers of any entries may be open at once, from any threads, but reading every entry that way costs the square of a folder's size. **`walk()`** reads them all in the archive's order with one decoder a folder: `for (auto [e, r] : a.walk())` gives each entry with a reader of its data. A reader serves until the walk goes on, which drops what the reader has not read of its entry and makes the reader `io::errc::closed`. An entry the walk cannot read (encrypted with no password or a wrong one, an unsupported method, a damaged folder) has a reader that fails, and the walk goes on to the next.
- **Checked.** The signature header, the header and a packed header have CRC-32s; every entry's data is counted to its size and checked against its CRC-32, and a folder with a CRC of its own is checked at its end (`errc::checksum`). Every coder of a folder must end where the header says its output ends — LZMA's and PPMd's range coders at zero, LZMA2's end byte, Deflate's last block, BZip2's end of stream, every stream of BCJ2 used up — as 7-Zip checks it, so a damaged stream that decodes to the right bytes and goes on is `errc::corrupt`.
- **Refused before anything is allocated:** a count in the header (files, folders, coders, bind pairs, streams) larger than the bytes of the header can hold, more entries than `limits.max_entries` (1 000 000), a header past 64 MiB (packed or not), a folder whose decoders need more than `limits.max_memory` (1 GiB; LZMA's and LZMA2's dictionaries are counted as large as the folder's output when that is smaller, PPMd's memory as its properties say), and in `read(e)` an entry larger than `limits.max_size`.
- **Passwords.** An archive with a password is read with the options: `archive::open(path, {.password = secret})`, or `extract(path, directory, {.password = secret})`. `options::password` is an `optional<slice<const byte>>`: the caller's bytes (a [`crypto::secret_bytes`](../crypto/secret.md#secret_bytes), a buffer or a literal), pointed to and never copied into managed memory. They are read when the call is made — turned then into the key derivation's UTF-16LE in plain memory, zeroed when the archive goes — so they have to live until that call and not after; the `async_` forms do it before the task is made. 7zAES is AES-256 in CBC mode, its key made from the password — given in UTF-8, hashed in UTF-16LE as 7-Zip hashes it — by 2^k rounds of SHA-256 over a salt, the password and the round's number. A key is made once for each salt and k and kept with the archive (7-Zip's archives and ours use one salt for every folder: one derivation an archive), in a [`crypto::secret`](../crypto/secret.md); the password's bytes are zeroed when the archive goes.
  - **No password:** an encrypted entry is listed, with `encrypted`, and reading it is `errc::password_required` ("7z: password required"); when the header is encrypted too (7-Zip's `-mhe=on`), the names are hidden and opening the archive is `errc::password_required`.
  - **A wrong password** is `errc::wrong_password` ("7z: wrong password"), the same error from `open` (an encrypted header), `read`, `reader` and `walk`. 7zAES has no check of the key: a wrong key decrypts to noise, which the decoders or the CRC-32 reject — and damaged encrypted data decrypts to noise just the same, so a damaged encrypted archive is a wrong password too (7-Zip says "Wrong password?" for both). Only failures of the data are said so; a folder whose coders cannot be made (unknown methods, damaged properties) says that.
  - **The rounds.** 7-Zip writes k = 19 (524 288 rounds, about 12 ms here); reading takes k up to 24 (about 0.4 s), and a larger k is `errc::too_large`, before any round is run — a header asking for 2^40 rounds would not finish. k = 63, the key the salt and the password themselves with no hashing, is read.
- **Deflate64** (`-m0=Deflate64`): DEFLATE with a window of 64 KB, lengths up to 65 538 and two more distance codes, decoded by the same decoder as Deflate, its window twice 64 KB. It is not written: `method` has no value for it.
- Methods the library does not know (Rar, ARJ, 7-Zip's own external codecs) are `errc::unsupported`, named in the error with their ID.
- `close()` closes the file the archive opened itself from a path; a file given to `open` is the caller's.
- **Tasks.** `async_open` reads the headers (and a packed header's streams) into managed memory the slices given to the file hold, then decodes from memory. A task's read of an entry of an archive in a file runs the decoding on the [blocking pool](../async/blocking.md), where the file is read: the job holds the entry's reader (a managed object, whose decoders' plain memory it owns) and the slice of the caller's buffer, so a task let go of meanwhile leaves the job nothing freed. An archive in memory is decoded on the worker, which it lets go every 64 KB.

### writer

```cpp
enum class method : uint8_t { lzma2, lzma, ppmd, deflate, copy };   // BZip2 and Deflate64 are read, not written
struct options {
    sevenzip::method method = sevenzip::method::lzma2;
    compress::level level;        // 0..9, 6 unless told otherwise
    bool solid = true;            // entries one after another in a folder
    uint64_t solid_block = 0;     // the data a folder takes before a new one starts; 0: 7-Zip's for the settings
    bool auto_filters = true;     // a branch converter or Delta by what each file's first bytes are
    optional<string> password;    // 7zAES: reading encrypted archives; writing, everything encrypted
    bool encrypt_header = true;   // with a password, the header too: the names hidden (7-Zip's -mhe=on)
    limits limit;                 // reading: the limits, as the (…, limits) forms take them
};
struct entry_info {
    optional<time::datetime> modified;    // none: now
    optional<time::datetime> created, accessed;
    optional<io::permissions> mode;       // none: 0644, a directory's 0755, a link's 0777
    bool symlink = false;                 // the data is the link's target
    optional<uint32_t> attributes;        // the archive's attributes as they are, past mode
};

explicit writer(const string& path);                  // and (path, options): a file made there, closed by close()
explicit writer(const io::file& file);               // and (file, options): from where the file stands; the caller's to close
explicit writer(const io::buffer& b);                // and (b, options): after what the buffer holds; the writer holds the buffer
io::writer create(const string& name);                // and (name, entry_info): the entry's data (the one before ended)
void add(const string& name, const slice<const byte>& data);            // and (name, data, entry_info): a whole entry
void add_directory(const string& name);               // and (name, entry_info)
expected<void, error> close();                        // + async_close
const optional<error>& last_error() const noexcept;   // the first error given, kept
```

Entries are written one after another; the writer is moved, not copied. `create` gives a writer of the entry's data (`io::writer`, as every stream of the library is written); the next `create`, `add` or `add_directory` ends the entry (its `close()` does too, and may be left out), and writing to an entry that was ended is `io::errc::closed`. An entry with no data is an empty file; a directory has none. The data goes through the folder's coders as it is written, in plain memory the writer keeps from folder to folder, and out to the file a quarter of a megabyte at a time: no entry is held whole. `close()` ends the last entry and folder and writes the header, packed with LZMA as 7-Zip packs it, and then the signature header at the archive's start — which is why a 7z archive needs an output that seeks back: a file (its start rewritten with `pwrite`), or an [`io::buffer`](../io/stream.md) (which seeks as a file does). A socket cannot take one.

- **Folders.** In a solid archive (the default) entries go one after another into a folder until it has taken `solid_block` bytes (by default 128 times the dictionary for LZMA and LZMA2, 16 times the model's memory for PPMd, within 16 MiB .. 4 GiB, and 16 MiB for Deflate and Copy, as 7-Zip reckons it), or until an entry calls for another filter; `solid = false` gives every entry a folder of its own. Entries are written in the order they come; 7-Zip also sorts them by extension first, which a program does itself by adding them in that order.
- **Automatic filters** (`auto_filters`, before every coder but Copy, as 7-Zip puts them): an entry's first 4 KB decide. A program — ELF, Mach-O or PE — gets the branch converter of the processor its header names: x86 and x86-64 BCJ, ARM, ARM-Thumb (PE's Thumb machines), ARM64, RISC-V, PowerPC and SPARC (big-endian ELF, Mach-O PowerPC), IA-64; a WAV file of PCM samples gets Delta by its block align (4 for 16-bit stereo). The name does not decide (a `.exe` that is no PE gets nothing), a universal Mach-O (two processors in one file) gets nothing, links and directories get nothing.
- **Levels.** LZMA and LZMA2 take xz's levels (the dictionary 256 KiB at 0 to 64 MiB at 9, the optimal parser from 4), with the dictionary written in the header no larger than the folder; PPMd takes 7-Zip's order and memory for the level (order 3..32, 512 KiB .. 192 MiB), Deflate the level as [flate](flate.md) takes it.
- **Entries as 7-Zip writes them on Unix:** names in UTF-16LE (the name given in UTF-8; a trailing `/` dropped; an empty name, a NUL or bytes that are not UTF-8 refused), the modification time (now, unless given), the attributes with the POSIX mode in the high 16 bits (0x8000 set, the type — file, directory, link — with the mode), a link's target as its data.
- Every error the writer gives is kept as its first: a failure of the output (a file that cannot be made at the path included), and the caller's too — a name it refuses, data for a directory, a write to an entry that was ended, an entry after close. Every write, `add`, `add_directory` and `close()` after it gives that error at once and writes nothing, and `create` then gives an entry writer whose writes give it. So an archive is written freely and checked once, at the close; `last_error()` holds the error. After the first error, its own included, the writer refuses everything further and `close()` returns that error; whoever wants to react earlier checks `last_error()` after a `create` or the result of a single `write`.
- The encoder's memory (for LZMA2 at level 6 about 90 MiB, at 9 about 700 MiB, as xz's) is taken at the first folder and given back at `close()`, not when the collector takes the writer. A folder per entry (`solid = false`) starts the encoder over for every entry, which empties its hash table each time: tens of thousands of small entries are written much faster solid.
- **A password** (`options.password`): every folder is encrypted with 7zAES — k = 19 as 7-Zip writes it, a random salt of 16 bytes for the archive and a random IV of 16 bytes for each folder, from [`crypto::random`](../crypto/random.md) — and with `encrypt_header` (the default, as `-mhe=on`) the header too, so that the names cannot be read without the password; `encrypt_header = false` leaves the header plain, the names listed by anyone. The key is made once, when the writer is made (about 12 ms), and kept in plain memory the writer zeroes at `close()`; the writer keeps no copy of the password. Entries without data (directories, empty files) are in no folder: only an encrypted header hides them.
- **Tasks.** An entry writer's `async_write` encodes in portions of 64 KB and lets the worker go between them, and writes the output through a managed block the file's write holds; `async_close` does the last folder and the header the same way.

### extract

```cpp
expected<void, error> extract(const string& archive_path, const string& directory, const options& o = {});
async::task<expected<void, error>> async_extract(string archive_path, string directory, options o = {});
```

The archive unpacked under the directory. Of the options it reads the password and the limits (the rest is the writer's).

- **`extract`** writes every entry under the directory, which it makes when it is not there. It makes directories, writes files with their mode (the archive's POSIX mode, 0644 when it has none) and modification time, and makes symbolic links last, so that no file is written through a link the archive made. Anti-items are passed over, and a file already there is written over.
  - **What it refuses.** Every name is checked before anything is written: an absolute one, one with a `..` that climbs out, or one with a `\` is `errc::insecure_path` (Go's `ErrInsecurePath`), and nothing is written. A link whose target leaves the directory (absolute, or climbing out from the link's own directory) is `errc::insecure_path` when it is reached.
  - **Too large.** Past `limit.max_size` of the files together (1 GiB by default: an archive comes from outside; 0: none) it is `errc::too_large`, and nothing is written.
  - **Errors of the data** are those of reading: `errc::password_required`, `errc::wrong_password`, `errc::checksum` and the others.
- **`async_extract`** writes the files on the blocking pool; the password's keys are made at the call.

### create

```cpp
expected<void, error> create(const string& directory, const string& archive_path, const options& o = {});
async::task<expected<void, error>> async_create(string directory, string archive_path, options o = {});
```

```cpp
compress::sevenzip::create("photos", "photos.7z", {.level = 9, .solid = false});
```

The directory packed into a 7z file with the [writer](#writer)'s options: LZMA2 at level 6, solid, the filters chosen by what each file is, as 7-Zip writes it; a password encrypts it, the header too. The entries are named from the directory, not with it (`index.html`, `css/`, `css/site.css`), in lexical order, with their mode and time; a symbolic link is archived as a link; a socket, a device or a fifo is left out. A failure removes the half-made file.

- **`async_create`** writes on the blocking pool. A password's key is made at the call, on the caller's thread (about 12 ms), as `async_extract` makes its keys: the caller's bytes never go to the pool, and the task needs nothing of them.
- Tested both ways against 7-Zip's `7zz` (`tests/compress/files.cpp`).

## Examples

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    compress::sevenzip::writer w("secret.7z", {.password = "correct horse"});
    w.add("plan.txt", "meet at noon\n");
    w.close();

    compress::sevenzip::extract("secret.7z", "restored", {.password = "correct horse"});
    print("{}", io::read_text("restored/plan.txt").value_or(string("?")));
}
```

Output:

```text
meet at noon
```

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main(int argc, char** argv) {
    if (argc < 2) {
        println("usage: list <archive.7z>");
        return 2;
    }
    auto a = compress::sevenzip::archive::open(string(argv[1]));
    if (!a) {
        println(a.error().message());
        return 1;
    }
    for (auto [e, r] : a->walk()) {  // one decoder a folder
        if (e.is_directory || e.is_anti) {
            println("{}/", e.name);
            continue;
        }
        auto data = r.read_all();  // checked against the entry's CRC-32
        if (!data) {
            println("{}: {}", e.name, data.error().message());
            continue;
        }
        println("{} {} bytes{}", e.name, data->size(), e.is_local() ? "" : " (not a local name)");
    }
}
```

Sample output:

```text
docs/
docs/notes.txt 1300 bytes
readme.md 412 bytes
```

An archive written entry by entry and checked once, at the close:

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/core/core.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    compress::sevenzip::writer w("backup.7z", {.level = 9});
    w.add("notes.txt", "remember the milk\n");  // a text is its bytes
    w.add_directory("logs");
    io::writer log = w.create("logs/app.log");
    for (int i : range(3)) {
        log.write("a line of the log\n");  // written freely...
    }
    if (auto done = w.close(); !done) {  // ...and checked once, here
        println(done.error().message());
        return 1;
    }
    compress::sevenzip::archive a = compress::sevenzip::archive::open("backup.7z");
    for (auto [e, r] : a.walk()) {
        println("{} {}", e.name,
                e.is_directory ? string("(directory)") : r.read_all_text().value_or(string("?")));
    }
}
```

Output:

```text
notes.txt remember the milk

logs (directory)
logs/app.log a line of the log
a line of the log
a line of the log

```

An archive with a password, the names hidden too:

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    {
        // the names hidden too
        compress::sevenzip::writer w("secret.7z", {.password = "correct horse"});
        w.add("plan.txt", "meet at noon\n");
        if (auto done = w.close(); !done) {
            println(done.error().message());
            return 1;
        }
    }
    auto none = compress::sevenzip::archive::open("secret.7z");
    println("{}", none ? string("opened") : none.error().message());
    auto wrong = compress::sevenzip::archive::open("secret.7z", {.password = "battery staple"});
    println("{}", wrong ? string("opened") : wrong.error().message());
    compress::sevenzip::archive a =
        compress::sevenzip::archive::open("secret.7z", {.password = "correct horse"});
    println("{}", a.reader("plan.txt")->read_all_text().value_or(string("?")));
    println("{} entry", a.entries().size());
}
```

Output:

```text
offset 64: 7z: password required
offset 64: 7z: wrong password
meet at noon

1 entry
```

## See also

[The module](README.md); [`xz`](xz.md) and [`lzma`](lzma.md) for the methods; `tests/compress/sevenzip.cpp` (archives of every method made by 7-Zip and libarchive, read and compared with the files; passwords both ways, AES-256-CBC on NIST SP 800-38A, the key against one computed apart) and `tests/compress/fuzz/sevenzip_fuzz.cpp`, `sevenzip_writer_fuzz.cpp`, `sevenzip_aes_fuzz.cpp`.
