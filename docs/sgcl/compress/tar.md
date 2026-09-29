# sgcl::compress::tar

```cpp
#include "sgcl/compress/tar.h"   // or "sgcl/compress/compress.h", "sgcl/sgcl.h"

namespace sgcl::compress::tar {
    enum class kind : uint8_t { file, directory, symlink, hardlink, char_device, block_device, fifo };
    struct entry;
    class reader;   // io::reader: next() moves to an entry, read() gives its data
    class writer;   // io::writer: write_header(entry), then its data through write()
    struct options; // extract's max_size, create's level

    expected<void, error> extract(const string& archive, const string& directory, const options& o = {});   // + async_extract
    expected<void, error> create(const string& directory, const string& archive, const options& o = {});    // + async_create
}
```

tar as POSIX and GNU have it: ustar, pax (a record per field, for names, sizes and times ustar cannot hold) and GNU's long names and links, read from any of them and written as ustar, or as pax where ustar cannot say what an entry holds. `.tar`, and with [gzip](gzip.md) `.tar.gz`.

## extract, create

```cpp
struct options {
    compress::level level;                   // create: gzip's or xz's level, 6 by default
    uint64_t max_size = limits{}.max_size;   // extract: the files' bytes together past which nothing is written; 1 GiB, 0: none
};

expected<void, error> extract(const string& archive_path, const string& directory, const options& o = {});
async::task<expected<void, error>> async_extract(string archive_path, string directory, options o = {});
expected<void, error> create(const string& directory, const string& archive_path, const options& o = {});
async::task<expected<void, error>> async_create(string directory, string archive_path, options o = {});
```

```cpp
compress::tar::extract("upload.tar.gz", "incoming", {.max_size = 100 << 20});
compress::tar::create("site", "site.tar.xz", {.level = 9});
```

- **`extract`** reads gzip, xz and bzip2 around the archive by its first bytes, whatever its name. **Every name is checked before anything is written**: an entry, or a link's target, that would leave the directory (the rule of [`io::path::is_local`](../io/path.md)) is `errc::insecure_path`, and nothing is written; so is a total size past `max_size` (`errc::too_large`), 1 GiB unless set, the module's [limits](README.md) as `decompress` has them: `tar -x` has no bound, this one has, since an archive comes from outside. Then directories (made with at least `rwx` for the owner), files with their mode and time, and symbolic and hard links, made last, so that no file is written through a link the archive made. A device or a fifo is left out; a file there already is written over. The archive is read twice for this, once for the names and once for the data.
- **`create`** takes what wraps the archive from its name: `.tar.gz` and `.tgz` gzip, `.tar.xz` and `.txz` xz, anything else none; `.tar.bz2` is `errc::unsupported` (bzip2 is read, not written). The entries are named from the directory, not with it (`index.html`, `css/`, `css/site.css`), as Go's `AddFS` names them, in lexical order, with their mode and time; a symbolic link is archived as a link, not followed; a socket, a device or a fifo is left out. A failure removes the half-made file.
- The `async_` forms run on the [blocking pool](../async/blocking.md). Tested both ways against bsdtar (`tests/compress/files.cpp`).

## entry

```cpp
struct entry {
    string name;
    string link_name;                   // the target of a symlink or a hard link
    tar::kind type = tar::kind::file;
    uint64_t size = 0;                  // the bytes of data: 0 for everything but a file
    io::permissions mode;
    time::datetime modified;            // UTC, to the nanosecond a pax record gives
    optional<time::datetime> accessed;  // when the archive has them (pax, GNU, star)
    optional<time::datetime> changed;
    int64_t uid = 0, gid = 0;
    string user_name, group_name;
    uint32_t dev_major = 0, dev_minor = 0;
    vector<pair<string, string>> pax;   // the pax records none of the fields holds, in order
    bool is_local() const;
};
```

What the headers before an entry's data say, merged into one: the ustar header, the archive's global pax records and the entry's own, GNU's long name and link. The names are the archive's as they are; **`is_local()`** says whether the name, and the link's target, stay inside the directory the archive is unpacked to (Go's `filepath.IsLocal`: not empty, not absolute, no `..` that climbs out, no backslash; a symlink's target taken from the link's directory, a hard link's from the archive's root). An archive from outside names its entries as it likes; a program writing them to disk checks each with it.

## reader

```cpp
explicit reader(const io::reader& in);
expected<optional<entry>, error> next();                 // the next entry, nullopt at the archive's end; + async_next
expected<size_t, io::error> read(const slice<byte>& out); // the current entry's data, exactly entry.size bytes; + async_read
const optional<error>& last_error() const noexcept;
expected<void, io::error> close();                       // closes in
```

`next()` skips what is left of the current entry's data and reads the next headers. Numbers in octal and in base-256 are read, the header's checksum both signed and unsigned (as old tars wrote it), and the archive ends, as in Go, at two blocks of zeros, or at one followed by the end of the input (a block of zeros followed by anything else is `errc::invalid_header`).

- **Read and not given:** a sparse file (GNU's and pax's forms) or a multi-volume part is `errc::unsupported` naming the entry, and `next()` goes on to the entry after it.
- **Limits:** a pax header or a GNU long name larger than 1 MiB is `errc::too_large`; a negative or impossible size is `errc::invalid_header`. Nothing an archive says makes the reader hold more than that.
- **Unlike Go:** the global pax records apply to the entries after them, as POSIX says (Go hands the `g` header out as an entry and applies nothing); a directory, a link or a device has size 0, whatever its header carries.

```cpp
compress::tar::reader r(compress::gzip::reader(io::open("site.tar.gz")));
while (auto e = r.next()) {
    if (!*e) break;                                     // the end
    if ((*e)->type == compress::tar::kind::file && (*e)->is_local()) {
        auto data = r.read_all();                       // the entry's bytes
        ...
    }
}
```

## writer

```cpp
explicit writer(const io::writer& out);
expected<void, error> write_header(const entry& e);      // + async_write_header
expected<size_t, io::error> write(const slice<const byte>& data);   // the entry's data, at most entry.size bytes in all
expected<void, io::error> close();                       // two blocks of zeros; out stays open
const optional<error>& last_error() const noexcept;     // the first error given, kept
```

An entry is its header, then exactly `size` bytes through `write` (more is `errc::invalid_argument`; a `write_header` or `close` before all of them is `errc::invalid_argument` too). The header is ustar when the entry fits it, and pax when it does not: a name or a link longer than ustar holds or not ASCII, a user or group name not ASCII, an id past 2 097 151, a size of 8 GiB or more, a time with a fraction of a second, before 1970 or past 2242, or any `accessed`, `changed` or `pax` record. The pax records are written sorted by key. The golden archives of Go's tests are matched byte for byte; the one known difference is the name of a pax header written for a directory (Go keeps the directory's trailing part in it). `write_header`, `write` and `close` have their `async_` forms. Every error the writer gives is kept as its first: a failure of `out`, and the caller's too (a header it cannot write, data past the size or before a header, a header or close before the data is whole, a call after close). Every `write_header`, `write` and `close` after it gives that error at once and writes nothing, so an archive is written freely and checked once, at the close; `last_error()` holds it. After the first error, its own included, the writer refuses everything further and `close()` returns that error; whoever wants to react earlier checks the result of `write_header` or of a single `write`, or `last_error()`.

## Example

```cpp
#include "sgcl/compress/compress.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    io::mkdir_all("site/css");
    io::write_file("site/index.html", "<h1>hello</h1>\n");
    io::write_file("site/css/site.css", "h1 { color: teal }\n");
    compress::tar::create("site", "site.tar.gz");      // gzip, by the name
    compress::tar::extract("site.tar.gz", "copy");     // gzip, xz or bzip2, by the first bytes
    print("{}", io::read_text("copy/index.html").value_or(string("?")));
}
```

Output:

```text
<h1>hello</h1>
```

