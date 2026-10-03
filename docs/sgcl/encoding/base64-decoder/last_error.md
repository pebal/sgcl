[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64/README.md) › [decoder](README.md)

# sgcl::encoding::base64::decoder::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Why the decoder's reads fail: the [error](../error/README.md) of the text, with its code and its offset in the text, or,
when the reader under it failed, an error of the code `io` whose `io_error()` is that reader's. Empty while
nothing failed. The failed read itself carries only an `io::error`, whose code is the [errc](../errc.md) code in
the `encoding` category; this is the place.

## Parameters

None.

## Return value

The error, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer text;
    text.write("aGVs\nbG8=");
    encoding::base64::decoder plain = encoding::base64::standard.decoder_from(text);
    println("{}", plain.last_error().has_value());
    auto all = plain.read_all();
    println("{}", plain.last_error()->message());
    println("{}", plain.last_error()->offset());
}
```

Output:

```text
false
offset 4: invalid character 0x0A
4
```

## See also

- [read, async_read](read.md): the reads that fail
- [lenient](../base64/lenient.md): a codec that takes the line ending
- [sgcl::encoding::base64::decoder](README.md)
