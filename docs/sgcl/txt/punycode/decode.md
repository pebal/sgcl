[sgcl](../../README.md) › [txt](../README.md) › [punycode](README.md)

# sgcl::txt::punycode::decode

```cpp
optional<string> decode(const string& label);
```

Returns one label read back from punycode, by [RFC 3492](https://www.rfc-editor.org/rfc/rfc3492), the `xn--` prefix
not part of it: the ASCII before the last `-` as it stands, the deltas after it decoded into the code points they
place. Every addition and multiplication is bounded before it is made, so a counter never wraps
([punycode](README.md)).

## Parameters

| Parameter | Description |
|---|---|
| `label` | one label in punycode, without `xn--` |

## Return value

The label in UTF-8, or nothing when it is not punycode: a digit that is not one, a number that stops in the middle, a
counter run past its bound, or a code point that no text can hold.

## Complexity

Linear in the length of the label for the reading, and `n log n` in the `n` code points it places: up to 64 of them
are placed by insertion, as the RFC draws it, and a longer one backwards over a tree of counts, so a long label from
outside costs no quadratic time.

## Exceptions

`length_error` when the result would pass the [max_size()](../../core/string/max_size.md) of a string.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto label : {"bcher-kva", "fa-hia", "abc-", "99999999", "bcher-k!a"}) {
        auto text = txt::punycode::decode(label);
        println("{} -> {}", label, text ? *text : "nothing");
    }
}
```

Output:

```text
bcher-kva -> bücher
fa-hia -> faß
abc- -> abc
99999999 -> nothing
bcher-k!a -> nothing
```

## See also

- [encode](encode.md): the other way
- [to_unicode](../idna/to_unicode.md): a whole name, with the prefix
- [sgcl::txt::punycode](README.md)
