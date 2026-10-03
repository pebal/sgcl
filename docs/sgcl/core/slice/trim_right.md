[sgcl](../../README.md) › [core](../README.md) › [slice](../slice.md)

# sgcl::slice\<T\>::trim_right

```cpp
slice trim_right() const noexcept;                                       // (1)
slice trim_right(std::basic_string_view<CharT> chars) const noexcept;    // (2)
slice trim_right(std::u32string_view set) const noexcept;                // (3)
```

The text without the given characters at its end, a slice of the same owner, as [trim](trim.md) trims both ends:
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
    string line = "data\r\n";
    string number = "1.500";
    string_slice s = line;
    println("[{}] [{}]", s.trim_right(), number.as_slice().trim_right("0"));
}
```

Output:

```text
[data] [1.5]
```

## See also

- [trim](trim.md): both ends
- [trim_left](trim_left.md): the start
- [sgcl::slice\<T\>](../slice.md)
