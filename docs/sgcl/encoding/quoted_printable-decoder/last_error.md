[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](../quoted_printable/README.md) › [decoder](README.md)

# sgcl::encoding::quoted_printable::decoder::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Where the text went wrong, with its offset, or why the reader under it failed (`errc::io` with its `io_error()`); nothing while the reading goes well.

## Parameters

None.

## Return value

The [error](../error/README.md) of the read that failed, or nothing.

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
    io::buffer in;
    in.write("ok=ZZ");
    auto dec = encoding::quoted_printable::standard.decoder_from(in);
    println("{}", io::read_all(dec).has_value());
    println("{}", dec.last_error()->message());
}
```

Output:

```text
false
offset 2: invalid quoted-printable escape
```

## See also

- [read, async_read](read.md)
- [decoder](README.md)
