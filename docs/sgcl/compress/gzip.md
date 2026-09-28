# sgcl::compress::gzip

```cpp
#include "sgcl/compress/gzip.h"   // or "sgcl/compress/compress.h"

namespace sgcl::compress {
    struct gzip_header {
        string name;                             // ISO 8859-1 in the file, UTF-8 here
        string comment;
        optional<time::datetime> modified;
        vector<byte> extra;
        uint8_t os = 255;                        // 255: unknown
    };

    class gzip {
    public:
        using header = gzip_header;
        struct options { compress::level level; gzip::header header; };
        static constexpr single_member_t single_member{};
        class writer;   // as flate::writer, the header written before the first bytes
        class reader;   // as flate::reader, every member one after another, and header()

        static vector<byte> compress(const slice<const byte>& data);          // and with options, and text
        static expected<vector<byte>, error> decompress(const slice<const byte>& data, const limits& l = {});
    };
}
```

gzip (RFC 1952): DEFLATE between a header — a name, a comment, the time the data was modified, an extra field, the system that made it — and the CRC-32 and the length of the data, which the reader checks at the end. `.gz` files, `.tar.gz`, and HTTP's `Content-Encoding: gzip`.

## Rules

- **Several members are one stream.** `cat a.gz b.gz > c.gz` is valid gzip, and gunzip gives both parts one after another: so do `decompress` and the reader. Bytes after the last member that are not another one are `errc::invalid_header` (fewer than ten, `errc::unexpected_end`), as in Go. `gzip::reader(in, gzip::single_member)` stops after the first member; it reads its input a block at a time, so a format that keeps other data after the member gives the reader only the member's part ([io::limit_reader](../io/stream.md)).
- **The header's strings are ISO 8859-1** in the format (§2.3.1): the writer converts the program's UTF-8 and refuses a character past U+00FF, a NUL, or an extra field past 65 535 bytes with `errc::invalid_argument` at its first write (as Go does), and `compress` with `std::invalid_argument` (it returns no error); the reader converts back.
- `header()` is the header of the member being read, read now if it was not yet (the first read reads it as well); `modified` is `nullopt` where the file says 0.
- `decompress` takes the length the last member's trailer states as a hint for its first buffer, no larger than the data could make, and grows it when the hint is wrong (a member's length is kept modulo 2³²).

## reader

```cpp
explicit reader(const io::reader& in);
reader(const io::reader& in, gzip::single_member_t);
expected<gzip::header, error> header();                         // + async_header
expected<size_t, io::error> read(const slice<byte>& out);       // and every form of io::mixin::reader
const optional<error>& last_error() const noexcept;
expected<void, io::error> close();                              // closes in
void reset(const io::reader& in);
```

```cpp
#include "sgcl/io/buffered.h"                                   // io::buffered_reader, for the lines

compress::gzip::reader r(io::open("access.log.gz"));
if (auto h = r.header()) println("{}", h->name);           // "access.log"
io::buffered_reader lines(r);                                    // named: lines() reads through it
for (auto line : lines.lines()) {
    ...
}
```

## writer

```cpp
explicit writer(const io::writer& out);
writer(const io::writer& out, const options& o);
// write, flush, close, last_error, reset and their async_ forms: as flate::writer
```

```cpp
compress::gzip::header h;
h.name = "report.csv";
h.modified = time::now();
compress::gzip::writer w(io::create("report.csv.gz"), {.level = 9, .header = h});
```
