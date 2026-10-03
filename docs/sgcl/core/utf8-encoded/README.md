[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8/README.md)

# sgcl::utf8::encoded

```cpp
#include "sgcl/core/utf8.h"   // or "sgcl/core.h"

namespace sgcl {
    struct utf8 {
        struct encoded;
    };
}
```

`sgcl::utf8::encoded` is a code point as the bytes of its encoding, kept in the value with their number: the text a
code point is searched for as. It is what the `char32_t` overloads of `find`, `contains`, `split` and `replace` of a
string search for, and what a program writes where it needs the bytes of one character as a `std::string_view`
without a buffer of its own. A value that is not a scalar value is encoded as `utf8::replacement`.

## Rules

- A value of `max_width` bytes and a size, trivially copyable, `constexpr`: it lives anywhere and holds nothing.
- The view it gives points into the value: it is valid while the value lives, as a view of a local array.

## Member objects

| Member | Description |
|---|---|
| `bytes` | `char[utf8::max_width]`: the bytes of the encoding, the rest zero |
| `size` | `size_t`: the number of bytes of the encoding, 1 to 4 |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](utf8-encoded.md) | encodes a code point |

#### Observers

| Function | Description |
|---|---|
| [view, operator std::string_view](view.md) | the bytes of the encoding as a `std::string_view` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    utf8::encoded euro(U'€');
    println("{} bytes: {:x} {:x} {:x}", euro.size, euro.bytes[0], euro.bytes[1], euro.bytes[2]);

    std::string_view text = euro;
    string price = string::concat("12 ", text);
    println("{} ({} bytes)", price, price.size());
}
```

Output:

```text
3 bytes: e2 82 ac
12 € (6 bytes)
```

## See also

- [encode](../utf8/encode.md): the encoding written into a buffer of the caller's
- [width](../utf8/width.md): the bytes an encoding takes
- [sgcl::utf8](../utf8/README.md)
