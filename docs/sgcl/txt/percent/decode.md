[sgcl](../../README.md) › [txt](../README.md) › [percent](README.md)

# sgcl::txt::percent::decode

```cpp
optional<string> decode(const string& text) noexcept;
```

Returns the bytes of `text` with every `%` and the two hexadecimal digits after it replaced by the byte they
spell. Either case of the digits is read, as section 6.2.2.1 says a consumer must. Nothing comes back when a `%` is
not followed by two hexadecimal digits: a truncated escape is not a text with a stray per cent sign in it, it is a
text that was cut, and letting it through is how a path traversal gets past a check that ran before the decoding.
`+` is left as it is.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the escaped text |

## Return value

The bytes, or `nullopt` for a `%` without two hexadecimal digits; `text` itself when it has no escape.

## Complexity

Linear in the length of the text: a pass that checks and counts the escapes, then the bytes written once.

## Exceptions

None.

## Notes

What comes back is bytes and not text. An escape can spell a byte no UTF-8 has: a caller who knows they are UTF-8 has them; one who does not turns them into text with [txt::decode](../decode.md).

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::percent::decode("%C3%bc"));  // the two bytes of "ü"
    println("{}", txt::percent::decode("a+b%20c"));
    println("{} {}", txt::percent::decode("a%").has_value(),
            txt::percent::decode("%zz").has_value());
}
```

Output:

```text
"ü"
"a+b c"
false false
```

## See also

- [encode](encode.md)
- [txt::decode](../decode.md): bytes of an encoding as text
- [sgcl::txt::percent](README.md)
