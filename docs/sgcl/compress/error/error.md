[sgcl](../../README.md) › [compress](../README.md) › [error](../error.md)

# sgcl::compress::error::error

```cpp
error() noexcept = default;                                          // (1)
error(errc code, uint64_t offset) noexcept;                          // (2)
error(errc code, uint64_t offset, const string& detail) noexcept;    // (3)
error(const io::error& e, uint64_t offset) noexcept;                 // (4)
```

Constructs an error. The module makes its errors itself; a program makes one to compare with, or to report a
failure of its own in the module's terms (a format of its own over the module's streams). An error made here has a
place; one of the module's that has none (not from the data, [message](message.md)) is not equal to one made at
offset 0.

1. `errc::corrupt` at offset 0.
2. The code at the offset; the message says the code's own words (`"checksum mismatch"`).
3. The code at the offset, with the detail the message says in place of the code's words (`"gzip: CRC-32
   mismatch"`, `"zip: method 12 (bzip2)"`).
4. The source or the sink failed at the offset: `errc::io`, the stream's error kept ([io_error](io_error.md)).

## Parameters

| Parameter | Description |
|---|---|
| `code` | what went wrong |
| `offset` | the byte of the compressed input (of the archive) where it was found |
| `detail` | the sentence the message says in place of the code's words |
| `e` | the error of the stream that failed |

## Complexity

Constant; (3–4) a copy of the detail or of the stream's error.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", compress::error().message());
    println("{}", compress::error(compress::errc::checksum, 12).message());
    println("{}", compress::error(compress::errc::unsupported, 0, "mine: version 3").message());
    println("{}", compress::error(io::error(io::errc::closed, "read", "data"), 5).message());
}
```

Output:

```text
offset 0: corrupt data
offset 12: checksum mismatch
offset 0: mine: version 3
offset 5: input/output error: read data: stream closed
```

## See also

- [message](message.md): what the error says
- [sgcl::compress::error](../error.md)
