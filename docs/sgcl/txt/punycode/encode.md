[sgcl](../../README.md) › [txt](../README.md) › [punycode](../punycode.md)

# sgcl::txt::punycode::encode

```cpp
optional<string> encode(const string& label);
```

Returns one label written in punycode, by [RFC 3492](https://www.rfc-editor.org/rfc/rfc3492), without the `xn--`
prefix: the ASCII code points of the label first, then a `-` when there were any, then the others encoded as
deltas. A label of ASCII alone is itself with a `-` after it. Nothing is mapped or checked: the case, the
normalization and the rules of a domain name are [idna](../idna.md)'s.

## Parameters

| Parameter | Description |
|---|---|
| `label` | one label, UTF-8; an invalid byte is read as `U+FFFD` |

## Return value

The label in punycode, or nothing when a delta passes the 2³² the RFC gives it, which a label of some thousands of
characters with a supplementary code point in it reaches.

## Complexity

Quadratic in the number of code points of the label at worst: a pass over the label for each distinct code point
above ASCII.

## Exceptions

`length_error` when the result would pass the [max_size()](../../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto label : {"bücher", "faß", "münchen", "abc", "日本語"}) {
        println("{} -> {}", label, txt::punycode::encode(label).value());
    }
}
```

Output:

```text
bücher -> bcher-kva
faß -> fa-hia
münchen -> mnchen-3ya
abc -> abc-
日本語 -> wgv71a119e
```

## See also

- [decode](decode.md): the other way
- [to_ascii](../idna/to_ascii.md): a whole name, with the prefix
- [sgcl::txt::punycode](../punycode.md)
