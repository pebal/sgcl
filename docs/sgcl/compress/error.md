[sgcl](../README.md) › [compress](README.md)

# sgcl::compress::error

```cpp
#include "sgcl/compress/error.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class error;
}
```

`sgcl::compress::error` is what went wrong in compressed data or in an archive: the [code](errc.md), the byte of the
compressed input (of the archive) where it was found, the error of the stream underneath when one failed, and a
[message](error/message.md) that says all of it. It is the error of every format of the module, one type under each
format's name: `flate::error`, `gzip::error`, `zip::error`, `tar::error` and the others are aliases of it.

Data in memory is decompressed into an [expected](../core/expected.md)`<vector<byte>, compress::error>`: the bytes,
or the error that says why not. A reader of the module is an [io stream](../io/README.md), and its reads fail with an
`io::error` of the [compress category](compress_category.md) (`"read gzip: checksum mismatch"`); the reader's
`last_error()` keeps the whole `compress::error`, offset and detail included. The archives (`zip`, `tar`,
`sevenzip`) return it from every function that reads or writes one.

## Rules

- A value: copied, compared, held in an `expected`. It holds a [string](../core/string.md) (the detail) and an
  optional `io::error`, so it lives where a string may.
- The offset counts bytes from the start of the compressed input or of the archive.
- An error that did not come from the data has no place: a file that does not open, cannot be made, written or
  closed, a failure of what a writer writes into, a mistake of the calls to a writer, a name the archive does not
  hold, a limit the caller set on what is extracted. Its offset is 0, and its message is the words alone
  (`"zip: no entry none.txt"`, `"input/output error: open missing.zip: No such file or directory"`).
- A format that knows more says it in the message: a zip or tar error names its entry
  (`"zip: entry a/b.txt: CRC-32 mismatch"`), a gzip error what failed (`"gzip: CRC-32 mismatch"`).
- A default-constructed error is `errc::corrupt` at offset 0.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](error/error.md) | constructs an error of a code and an offset, with a detail, or of a stream's error |

#### Observers

| Function | Description |
|---|---|
| [code](error/code.md) | what went wrong, an [errc](errc.md) |
| [offset](error/offset.md) | the byte of the compressed input where it was found; 0 when it is not the data's |
| [io_error](error/io_error.md) | the stream's own error, when the source or the sink failed |
| [message](error/message.md) | the whole of it as a sentence: `"offset 20: gzip: CRC-32 mismatch"` |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](error/operator_cmp.md) | the same code, place, detail and stream error |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto packed = compress::gzip::compress("hello, hello, hello");
    packed[packed.size() - 8] ^= byte(1);  // a bit of the CRC-32 flipped

    auto back = compress::gzip::decompress(packed);
    if (!back) {
        const compress::error& e = back.error();
        println("{}", e.message());
        println("at byte {} of {}", e.offset(), packed.size());
    }
}
```

Output:

```text
offset 20: gzip: CRC-32 mismatch
at byte 20 of 28
```

## See also

- [errc](errc.md): the codes
- [compress_category](compress_category.md), [make_error_code](make_error_code.md): the code inside an `io::error`
- [io::error](../io/error.md): the error of the streams
- [sgcl::compress](README.md)
