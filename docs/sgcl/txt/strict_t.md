[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::strict_t

```cpp
#include "sgcl/txt/encoding.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct strict_t {
        explicit strict_t() = default;
    };
    inline constexpr strict_t strict{};
}
```

The tag that chooses the strict form of [decode](decode.md): `decode(bytes, from, txt::strict)` gives the text or the
first byte that means nothing in the encoding, as a [decode_error](decode_error.md), where the form without it
writes a `U+FFFD` for that byte and goes on.

## Rules

- An empty type, passed by value. The default constructor is explicit, so a tag is not made from a bare `{}`: it is
  written by its constant, `txt::strict`.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the default constructor, explicit |

## Non-member functions

#### Constants

| Constant | Value | Description |
|---|---|---|
| `strict` | `strict_t{}` | the strict form of `decode`, `inline constexpr strict_t` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

#include <type_traits>

using namespace sgcl;

int main() {
    byte bytes[] = {byte('o'), byte('k'), byte(0xC3)};
    println("{}", txt::decode(bytes, txt::encoding::utf8).size());
    auto checked = txt::decode(bytes, txt::encoding::utf8, txt::strict);
    println("byte {}: {}", checked.error().offset(), checked.error().message());
    println("{}", std::is_empty_v<txt::strict_t>);
}
```

Output:

```text
5
byte 2: not utf-8
true
```

## See also

- [decode](decode.md), [decode_error](decode_error.md)
- [sgcl::txt](README.md)
