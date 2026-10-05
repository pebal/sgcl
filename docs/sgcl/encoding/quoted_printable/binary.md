[sgcl](../../README.md) › [encoding](../README.md) › [quoted_printable](README.md)

# sgcl::encoding::quoted_printable::binary

```cpp
constexpr quoted_printable binary() const noexcept;
```

The same codec in the binary form: CR and LF bytes as any other, `=0D` and `=0A`; for content that is not text, where a line break of the input is not one of the output. The other choice, [lenient](lenient.md), stays as it was.

## Parameters

None.

## Return value

The codec with the choice made.

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
    auto bin = encoding::quoted_printable::standard.binary();
    println("{}", bin.encode("a\r\nb"));
    println("{} {}", bin.is_binary(), bin.is_lenient());
}
```

Output:

```text
a=0D=0Ab
true false
```

## See also

- [is_binary](is_binary.md)
- [lenient](lenient.md)
- [quoted_printable](README.md)
