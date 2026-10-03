[sgcl](../../README.md) › [encoding](../README.md) › [base64](../base64.md)

# sgcl::encoding::base64::without_padding

```cpp
constexpr base64 without_padding() const noexcept;
```

The same codec without padding: Go's `WithPadding(base64.NoPadding)`. Its encoding leaves the last group short,
two or three characters for one or two bytes, and its decoding refuses the padding a padded codec wants: `Zg` is
read and `Zg==` refused. The alphabet and the strictness stay as they are. `raw_standard` and `raw_url` are
`standard.without_padding()` and `url.without_padding()`.

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
    auto raw = encoding::base64::standard.without_padding();
    println("{} {}", encoding::base64::standard.encode("f"), raw.encode("f"));
    println("{}", raw.decode("Zg==").error().message());
    println("{}", raw.encode("f") == encoding::base64::raw_standard.encode("f"));
}
```

Output:

```text
Zg== Zg
offset 2: invalid character '='
true
```

## See also

- [padded](padded.md): whether a codec pads
- [lenient](lenient.md): the other choice of a codec
- [sgcl::encoding::base64](../base64.md)
