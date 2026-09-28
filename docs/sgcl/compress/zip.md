# sgcl::compress::zip

```cpp
#include "sgcl/compress/zip.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress::zip {
    enum class method : uint16_t { store = 0, deflate = 8, deflate64 = 9 };   // deflate64: read, not written
    struct entry;
    class archive;   // an archive to read: its entries in any order
    class writer;    // an archive written entry after entry
}
```

zip as PKWARE's APPNOTE 6.3.10 has it and as every tool writes it: entries stored or deflated, ZIP64 for more than 65 535 entries and past 4 GiB, names in UTF-8; entries of Deflate64 (method 9, as 7-Zip and Windows write large archives) are read. `.zip`, `.jar`, `.docx`, `.apk`.

## entry

```cpp
struct entry {
    string name;                  // "dir/file.txt", '/' between the parts; a directory ends in '/'
    string comment;
    time::datetime modified;
    uint64_t size = 0;            // decompressed
    uint64_t compressed_size = 0;
    uint32_t crc32 = 0;
    zip::method method = zip::method::deflate;
    io::permissions mode = io::permissions(0644);
    bool symlink = false;         // the data is the target
    vector<byte> extra;           // the central record's extra field, as it is
    uint64_t offset = 0;          // where its local header starts in the archive
    bool is_directory() const noexcept;
    bool is_symlink() const noexcept;
    bool is_local() const noexcept;
};
```

What the entry's central directory record says. The time is the one of the extra fields that carry one (the extended timestamp, Info-ZIP's Unix field, NTFS's), else the DOS date and time of the record, which name no zone and are read as UTC, as Go reads them. A name is UTF-8 when the record says so or when it is valid UTF-8 anyway (most tools write it without the flag); anything else is taken as code page 437, which is what the flag's absence means.

**`is_local()`** says whether the name may be joined to a directory without leaving it: not empty, not absolute, no NUL, no `\`, and no `..` that climbs above the start (`a/../b` is local, `a/../..` is not): Go's `filepath.IsLocal`, and the rule of tar's entries too. An archive from outside names its entries as it likes (`../../etc/passwd`, the *zip slip*); a program that writes entries to disk checks each name with it, until the module has an extract that does it.

## archive

```cpp
static expected<archive, error> open(const string& path);                     // + async_open
static expected<archive, error> open(const io::file& file);                   // + async_open
static expected<archive, error> from(const slice<const byte>& data);          // an archive in memory (an unmanaged buffer is the caller's to keep alive)
slice<const entry> entries() const noexcept;
optional<entry> find(const string& name) const;                               // the first of that name
expected<io::reader, error> reader(const entry& e) const;                     // and (name)
expected<vector<byte>, error> read(const entry& e, const limits& l = {}) const;   // and (name, l); + async_read
const string& comment() const noexcept;
expected<void, error> close();
```

Opening reads the central directory, where the archive lists its entries, in pieces of 256 KB, and nothing else; an entry's data is read when it is asked for, through `reader(e)` as a stream or whole with `read(e)`. Readers of any number of entries may be open at once, from any threads (the file is read at offsets). An archive is a value, copied cheaply: the entries are shared.

- **Checked against the directory.** An entry's sizes and CRC-32 are the central record's (a failure read through the io handle names the entry as its path): its reader gives exactly `e.size` bytes, and fewer or more is `errc::corrupt`, a wrong CRC-32 `errc::checksum`, at the read that reaches the end. `read(e)` refuses an entry larger than the [limit](README.md#limits) before reading a byte, so that neither a zip bomb nor entries overlapping the same data (the "overlap" bomb) take more than the limit.
- The local header is read only to find where the data starts (its own name and extra field lengths, which may differ from the central record's); its name must be the central one.
- **Tolerated as Go tolerates it:** bytes in front of the archive (a self-extractor), a directory offset or size the end record gets wrong, junk after the archive, data descriptors with or without their signature.
- **Deflate64** (method 9, PKWARE's "enhanced deflate"): DEFLATE with a window of 64 KB, lengths up to 65 538 and two more distance codes. Its entries are read as deflated ones are, the reader's window twice 64 KB; the writer does not make them (`create` with it is `errc::unsupported`, "only store and deflate are written").
- **Refused:** a comment that runs past the end of the file (a truncated archive), a count of entries the directory does not hold, encryption and methods other than store, deflate and Deflate64 (`errc::unsupported`, when the entry is read).
- `close()` closes the file the archive opened itself from a path; a file given to `open` is the caller's.
- A task's reads of a file (`async_open`, `async_read`, an entry reader's `async_read`) run on the [blocking pool](../async/blocking.md), so they go into managed memory a slice given to the file holds: one buffer for the open, a block of 512 bytes an entry reader keeps for the local header and the name, the inflater's input, and for a stored entry the caller's own slice, its owner with it. A thread's reads keep plain memory, and so does a task's of an archive in memory, which waits for nothing and is read as a thread reads it (letting the worker go every 64 KB). A task's writes (`async_create`, `async_add`, `async_close`) hand out the headers, the descriptors and the directory from a managed block the writer keeps; a task's `create` writes its entry's local header with the entry's first bytes, in one write, whose failure is kept as any other.

```cpp
auto a = compress::zip::archive::open("site.zip");
if (!a) return fail(a.error());
for (auto& e : a->entries()) {
    if (e.is_directory() || !e.is_local()) continue;
    auto data = a->read(e);                    // vector<byte>, under 1 GiB
    ...
}
```

## writer

```cpp
explicit writer(const io::writer& out);
expected<io::writer, error> create(const string& name);        // a deflated entry, modified now ("dir/": a directory)
expected<io::writer, error> create(const entry& e);            // with e's method, time, mode, comment; + async_create(entry)
expected<void, error> add(const string& name, const slice<const byte>& data);   // a whole entry; + async_add
expected<void, error> set_comment(const string& comment);
expected<void, error> close();                                  // + async_close
const optional<error>& last_error() const noexcept;             // the first error given, kept
```

Entries are written one after another, as Go's `zip.Writer` has them; the writer is moved, not copied. `create` gives a writer of the entry's data; the next `create` ends the entry (its `close()` does too, and may be left out), and writing to an entry that was ended is `io::errc::closed`. `close()` ends the last entry and writes the central directory, with the ZIP64 records when there are 65 535 entries or more or an offset past 4 GiB, and leaves `out` open. The local headers carry no sizes (a data descriptor after the data has them), so `out` is written straight through and never needs to seek: a socket, an HTTP response.

Every error the writer gives is kept as its first: a failure of `out`, and the caller's too — a write to an entry that was ended, data for a directory, an entry `create` refuses (a name too long, a method not written), a create after close, a comment too long. Every write, `add`, `set_comment` and `close()` after it gives that error at once and writes nothing, and `create` then gives an entry writer whose writes give it. So an archive is written freely and checked once, at the close; `last_error()` holds the error. After the first error, its own included, the writer refuses everything further and `close()` returns that error; whoever wants to react earlier checks `last_error()` after a `create` or the result of a single `write`.

```cpp
compress::zip::writer w(io::create("backup.zip"));
w.add("notes.txt", "remember the milk\n");      // a text is its bytes, a vector<byte> likewise
io::writer log = w.create("logs/app.log");
for (auto& line : lines) log.write(line);
if (auto r = w.close(); !r) println(r.error().message());   // a failure of any step above
```
