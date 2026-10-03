[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](README.md)

# sgcl::compress::sevenzip::archive::open, async_open

```cpp
static expected<archive, error> open(const string& path) noexcept;                            // (1)
static expected<archive, error> open(const string& path, const limits& l) noexcept;           // (2)
static expected<archive, error> open(const string& path, const options& o) noexcept;          // (3)
static async::task<expected<archive, error>> async_open(string path) noexcept;                // (4)
static async::task<expected<archive, error>> async_open(string path, limits l) noexcept;      // (5)
static async::task<expected<archive, error>> async_open(string path, options o) noexcept;     // (6)
static expected<archive, error> open(const io::file& file) noexcept;                          // (7)
static expected<archive, error> open(const io::file& file, const limits& l) noexcept;         // (8)
static expected<archive, error> open(const io::file& file, const options& o) noexcept;        // (9)
static async::task<expected<archive, error>> async_open(io::file file) noexcept;              // (10)
static async::task<expected<archive, error>> async_open(io::file file, limits l) noexcept;    // (11)
static async::task<expected<archive, error>> async_open(io::file file,                        // (12)
                                                        options opt) noexcept;
```

Opens an archive: reads the signature header and the header at the end — decoding it first when it is packed, and
decrypting it when it is encrypted — and nothing else. The entries' data is read when it is asked for.

- (1–6) The file at `path`, opened by the archive and closed by its [close](close.md).
- (7–12) A file the program opened, which stays the program's to close.
- (1), (4), (7), (10) With the default [limits](../limits.md) and no password.
- (2), (5), (8), (11) With the limits given: `max_entries` and `max_memory` hold the header, and the limits stay the
  archive's for its reads.
- (3), (6), (9), (12) With the password and the limits (`limit`) of the [options](../sevenzip-options.md); the rest
  of the options is the writer's. The password's bytes are read at the call, the `async_` forms' too: the keys are
  made then, and the task holds them.
- (4–6), (10–12) Return a task whose reads of the signature header and the header (and a packed header's streams) go
  into managed memory the slices given to the file hold, on the [blocking pool](../../async/spawn_blocking.md), then
  decode from memory.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the archive's path |
| `file` | an open file of the archive |
| `l` | the bounds of the header and of the reads ([limits](../limits.md)) |
| `o`, `opt` | the password and the limits |

## Return value

The archive, or the [error](../error/README.md): not 7z (`errc::invalid_header`), a CRC-32 of a header that does not match
(`errc::checksum`), a header the format does not allow or a name that is not UTF-16 (`errc::corrupt`), a count, a
header or a key's rounds past the limits (`errc::too_large`), an encrypted header without a password
(`errc::password_required`) or with a wrong one (`errc::wrong_password`), a failure of the file (`errc::io`).

## Complexity

Linear in the size of the header.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    {
        // the names hidden too
        compress::sevenzip::writer w("secret.7z", {.password = "correct horse"});
        w.add("plan.txt", "meet at noon\n");
        (void)w.close();
    }
    auto none = compress::sevenzip::archive::open("secret.7z");
    println("{}", none.error().message());
    auto wrong = compress::sevenzip::archive::open("secret.7z", {.password = "battery staple"});
    println("{}", wrong.error().message());
    auto a = compress::sevenzip::archive::open("secret.7z", {.password = "correct horse"});
    println("{}", a->reader("plan.txt")->read_all_text().value_or(string("?")));
    (void)a->close();
    (void)io::remove("secret.7z");
}
```

Output:

```text
offset 64: 7z: password required
offset 64: 7z: wrong password
meet at noon

```

## See also

- [from](from.md): an archive in memory
- [sgcl::compress::sevenzip::archive](README.md)
