[sgcl](../../../README.md) › [core](../../README.md) › [mixin](../README.md) › [text](README.md)

# sgcl::mixin::text\<Derived, CharT, Traits\>::copy

```cpp
size_type copy(CharT* dest, size_type n, size_type pos = 0) const;
```

Copies at most `n` characters from position `pos` into the array at `dest`, as `std::basic_string_view::copy` does:
the characters `[pos, pos + min(n, size() - pos))`. No NUL is written after them.

## Parameters

| Parameter | Description |
|---|---|
| `dest` | the array the characters are copied into, with room for them |
| `n` | the most characters to copy |
| `pos` | the position of the first character to copy |

## Return value

The number of characters copied.

## Complexity

Linear in the number of characters copied.

## Exceptions

`out_of_range` when `pos > size()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <string_view>

using namespace sgcl;

int main() {
    string s = "the quick brown fox";
    char buffer[8] = {};
    size_t copied = s.copy(buffer, 5, 4);
    println("{} {}", copied, buffer);

    copied = s.copy(buffer, 7, 16);  // three are left from 16
    println("{} {}", copied, std::string_view(buffer, copied));

    try {
        s.copy(buffer, 1, 20);
    } catch (const out_of_range&) {
        println("out of range");
    }
}
```

Output:

```text
5 quick
3 fox
out of range
```

## See also

- [str](str.md): the characters as a `std::basic_string`
- [view, operator view_type](view.md): the characters as a view, nothing copied
- [sgcl::mixin::text\<Derived, CharT, Traits\>](README.md)
