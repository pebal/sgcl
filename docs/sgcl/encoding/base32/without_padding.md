[sgcl](../../README.md) › [encoding](../README.md) › [base32](README.md)

# sgcl::encoding::base32::without_padding

```cpp
constexpr base32 without_padding() const noexcept;
```

The same codec without padding: Go's `WithPadding(base32.NoPadding)`. Its encoding leaves the last group short,
the 2, 4, 5 or 7 characters that carry 1 to 4 bytes, and its decoding refuses the padding a padded codec wants.
The alphabet and the strictness stay as they are. The secret of a one-time password is written so.

## Parameters

None.

## Return value

The codec without padding.

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
    auto raw = encoding::base32::standard.without_padding();
    println("{} {}", encoding::base32::standard.encode("f"), raw.encode("f"));
    println("{}", raw.decode("MY======").error().message());
}
```

Output:

```text
MY====== MY
offset 2: invalid character '='
```

## See also

- [padded](padded.md): whether a codec pads
- [lenient](lenient.md): the other choice of a codec
- [sgcl::encoding::base32](README.md)
