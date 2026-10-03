[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::trim

```cpp
/*(1)*/ slice trim() const noexcept;
/*(2)*/ slice trim(std::basic_string_view<CharT> chars) const noexcept;
/*(3)*/ slice trim(std::u32string_view set) const noexcept;
```

The text without the given characters at both ends, a slice of the same owner, nothing copied. `CharT` is the
character type of a `slice<const CharT>`; the members take part only for a slice of a character type.

1. Without white space: Unicode's, a code point at a time in UTF-8 (`unicode::is_space`), a unit at a time in a wide
   text.
2. Without the characters of `chars`, each a code unit.
3. Without the code points of `set`, the text walked by code points: `trim(U"«» ")`. Not for a slice of `char32_t`,
   whose (2) takes code points.

A text that is all trimmed gives an empty slice, still of the same owner.

## Parameters

| Parameter | Description |
|---|---|
| `chars` | the code units to trim |
| `set` | the code points to trim |

## Return value

A slice of the characters that remain, with the same owner.

## Complexity

Linear in the characters trimmed.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = " \t value \n";
    string dashed = "--value--";
    string quoted = "«quoted»";
    string_slice s = text;
    println("[{}] [{}] [{}]", s.trim(), dashed.as_slice().trim("-"), quoted.as_slice().trim(U"«»"));
}
```

Output:

```text
[value] [value] [quoted]
```

## See also

- [trim_left](trim_left.md), [trim_right](trim_right.md): one end only
- [trim_prefix](trim_prefix.md), [trim_suffix](trim_suffix.md): a given prefix or suffix
- [sgcl::slice\<T\>](../slice.md)
