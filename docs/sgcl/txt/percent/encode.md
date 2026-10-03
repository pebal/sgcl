[sgcl](../../README.md) › [txt](../README.md) › [percent](README.md)

# sgcl::txt::percent::encode

```cpp
string encode(const string& text, percent_set keep = unreserved);
```

Returns `text` with every byte outside the set `keep` written as `%` and two hexadecimal digits. The digits are
upper case, which section 6.2.2.1 of RFC 3986 says a producer should use, and the bytes are the text's own: UTF-8,
one escape a byte, which is what a URL carries; a byte above ASCII is never in a set. This cannot fail: a byte is
either left alone or written as three characters, and every byte has a spelling. A space is `%20`, never `+`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, or any bytes in a string |
| `keep` | the characters left alone; [unreserved](README.md#member-objects) by default |

## Return value

The escaped text; `text` itself when nothing is escaped.

## Complexity

Linear in the length of the text: a pass that counts the escapes, then the text written once.

## Exceptions

`length_error` when the result would pass a string's `max_size()`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::percent::encode("/a b/c", txt::percent::path));
    println("{}", txt::percent::encode("/a b/c"));
    println("{}", txt::percent::encode("żółw"));
    auto mine = txt::percent::unreserved | txt::percent_set("/:");
    println("{}", txt::percent::encode("a b/c:d?", mine));
}
```

Output:

```text
/a%20b/c
%2Fa%20b%2Fc
%C5%BC%C3%B3%C5%82w
a%20b/c:d%3F
```

## See also

- [decode](decode.md): the way back
- [percent_set](../percent_set/README.md)
- [sgcl::txt::percent](README.md)
