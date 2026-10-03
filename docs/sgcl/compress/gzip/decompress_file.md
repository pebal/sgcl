[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip.md)

# sgcl::compress::gzip::decompress_file, async_decompress_file

```cpp
static expected<void, error> decompress_file(const string& path);                            // (1)
static expected<void, error> decompress_file(const string& path, const file_options& o);     // (2)
static async::task<expected<void, error>> async_decompress_file(string path) noexcept;       // (3)
static async::task<expected<void, error>> async_decompress_file(string path,                 // (4)
                                                                file_options o) noexcept;
```

What gunzip does to a file: takes a name ending in `.gz` and writes the name without it, every member one after
another, with the time and the mode of the `.gz`. The name in the header is not read, as gunzip reads it only with
`-N`; for a `.gz` that [compress_file](compress_file.md) made the two are the same, a name past ISO 8859-1 too. A
file there already is written over. A file that is not gzip, or is damaged, leaves nothing behind: the half-made file
is removed. The `.gz` stays unless `keep` is `false`.

- (1), (3) The `.gz` kept.
- (2), (4) With the [file_options](../gzip-file_options.md) given; the level is not read.
- (3–4) Return a task that runs the work on the [blocking pool](../../async/spawn_blocking.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the `.gz` file |
| `o` | whether the `.gz` stays |

## Return value

Nothing, or the [error](../error.md): the data's error (as [decompress](decompress.md) gives it, at its offset in the
`.gz`), a read of the `.gz` that fails (`errc::io`, at the bytes read before it), a name that does not end in `.gz`
(`errc::invalid_argument`), a failure of the file system (`errc::io`: the `.gz` does not open, the output cannot be
made or written). The last two have no place, as they are not the data's: their message is the words alone.

## Complexity

Linear in the size of the output.

## Exceptions

- (1–2) What the reads and writes of the files throw; their errors are returned.
- (3–4) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> unpack() {
    auto done = co_await compress::gzip::async_decompress_file("notes.txt.gz", {.keep = false});
    println("{}", done.has_value());
    println("{}", compress::gzip::decompress_file("notes.txt").error().message());
}

int main() {
    (void)io::write_file("notes.txt.gz", compress::gzip::compress("some notes\n"));
    async::spawn(unpack()).wait();
    print("{}", io::read_text("notes.txt").value_or(string("?")));
    println("{}", io::exists("notes.txt.gz"));
    (void)io::remove("notes.txt");
}
```

Output:

```text
true
gzip: a name that does not end in .gz: notes.txt
some notes
false
```

## See also

- [compress_file](compress_file.md): the other way
- [file_options](../gzip-file_options.md)
- [sgcl::compress::gzip](../gzip.md)
