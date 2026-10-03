[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md)

# sgcl::compress::gzip::compress_file, async_compress_file

```cpp
static expected<void, error> compress_file(const string& path);                            // (1)
static expected<void, error> compress_file(const string& path, const file_options& o);     // (2)
static async::task<expected<void, error>> async_compress_file(string path) noexcept;       // (3)
static async::task<expected<void, error>> async_compress_file(string path,                 // (4)
                                                              file_options o) noexcept;
```

What gzip(1) does to a file: writes `path + ".gz"` beside it, with the file's name (without its directory) in the
header — ISO 8859-1 where it holds the name, else the name's own bytes, as gzip(1) writes them
([gzip_header](../gzip_header.md)) — the file's time in the header and on the new file, and the file's mode on the new file; the header's system
is 3, Unix. A `.gz` there already is written over. The original stays unless `keep` is `false`, which removes it
once the `.gz` is whole, as gzip without `-k`. A failure removes the half-made `.gz`.

- (1), (3) At level 6, the original kept.
- (2), (4) With the [file_options](../gzip-file_options.md) given.
- (3–4) Return a task that runs the work on the [blocking pool](../../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file to compress |
| `o` | the level, and whether the original stays |

## Return value

Nothing, or the [error](../error.md): a failure of the file system (`errc::io`, the `io::error` in
[io_error](../error/io_error.md)): the file missing, the `.gz` not writable, the original not removable. No data is
read, so the error has no place: its message is `input/output error: ` and the stream's message.

## Complexity

Linear in the size of the file.

## Exceptions

- (1–2) What the reads and writes of the files throw; their errors are returned.
- (3–4) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("report.csv", string("day,visits\nmonday,12\n").repeat(100));
    if (auto done = compress::gzip::compress_file("report.csv"); !done) {
        println("{}", done.error().message());
        return 1;
    }
    println("{} {}", io::exists("report.csv"), io::exists("report.csv.gz"));

    io::reader file = *io::open("report.csv.gz");
    compress::gzip::reader r(file);
    println("{}", r.header()->name);
    (void)r.close();
    (void)io::remove("report.csv");
    (void)io::remove("report.csv.gz");
}
```

Output:

```text
true true
report.csv
```

A name past ISO 8859-1, there and back:

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("prices in €.txt", string("croissant, 4.50\n"));
    (void)compress::gzip::compress_file("prices in €.txt", {.keep = false});

    io::reader file = *io::open("prices in €.txt.gz");
    compress::gzip::reader r(file);
    println("{}", r.header()->name);
    (void)r.close();

    (void)compress::gzip::decompress_file("prices in €.txt.gz", {.keep = false});
    print("{}", io::read_text("prices in €.txt").value_or(string("?")));
    (void)io::remove("prices in €.txt");
}
```

Output:

```text
prices in €.txt
croissant, 4.50
```

## See also

- [decompress_file](decompress_file.md): the other way
- [file_options](../gzip-file_options.md)
- [sgcl::compress::gzip](../gzip.md)
