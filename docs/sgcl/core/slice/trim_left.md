[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::trim_left

```cpp
slice trim_left() const noexcept;                                       // (1)
slice trim_left(std::basic_string_view<CharT> chars) const noexcept;    // (2)
slice trim_left(std::u32string_view set) const noexcept;                // (3)
```

The text without the given characters at its start, a slice of the same owner, as [trim](trim.md) trims both ends:
white space (1), the code units of `chars` (2), the code points of `set` (3), the text walked by code points; (3)
not for a slice of `char32_t`, whose (2) takes code points. The members take part only for a slice of a character
type.

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
    string text = "  indented  ";
    string number = "0042";
    string_slice s = text;
    println("[{}] [{}]", s.trim_left(), number.as_slice().trim_left("0"));
}
```

Output:

```text
[indented  ] [42]
```

## See also

- [trim](trim.md): both ends
- [trim_right](trim_right.md): the end
- [sgcl::slice\<T\>](README.md)
