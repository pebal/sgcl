[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::at

```cpp
const CharT& at(size_type i) const;
```

Returns a reference to the character at `i`, with bounds checking: an `i` outside the text throws. The characters
are read-only: a string never changes, and a text slice reads them through the mixin as `const`.

## Parameters

| Parameter | Description |
|---|---|
| `i` | the position of the character, a unit of `CharT` (a byte in a UTF-8 text) |

## Return value

A reference to the character.

## Complexity

Constant.

## Exceptions

`out_of_range` when `i >= size()`.

## Notes

In a UTF-8 text the character at a position is a byte, part of a code point that may take up to four;
[decode](decode.md) reads the code point at a byte position.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string s = "hello";
    println("{} {}", s.at(1), s.as_slice(3).at(1));

    try {
        s.at(5);
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
e o
out of range: sgcl::text::at
```

## See also

- [decode](decode.md): the code point at a byte position
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)
