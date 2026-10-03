[sgcl](../../README.md) › [core](../README.md) › [string](../string.md)

# sgcl::string::substr

```cpp
basic_string substr(size_type pos = 0, size_type n = npos) const;
```

Returns a new string of the characters `[pos, pos + n)`, `n` cut to the size: `substr(pos)` is every character from
`pos` on, and `substr(size())` is the empty string. When the range is the whole string (`pos` 0 and `n` at least
`size()`), the result is the same object, with no copy.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the first character |
| `n` | the number of characters, at most `size() - pos` |

## Return value

A string of the characters, or this string's object when the range is the whole of it.

## Complexity

Linear in the number of characters copied: one allocation; constant for the whole string and for an empty range.

## Exceptions

`out_of_range` when `pos > size()`. The string is unchanged.

## Notes

A part that is only read needs no copy: [as_slice](as_slice.md) gives it as a slice that holds the string's object,
in constant time and with no allocation. A slice holds the whole object, though, so a short part kept for long keeps
the whole text alive; `substr` makes a string of its own, its object no larger than its characters.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string line = "hello, world";
    println("{} {} {}", line.substr(7), line.substr(0, 5), line.substr(line.size()).empty());
    string whole = line.substr(), world = line.substr(7);
    println("{} {}", whole.object() == line.object(), world.object() == line.object());

    try {
        line.substr(20);
    } catch (const out_of_range&) {
        println("out_of_range");
    }
}
```

Output:

```text
world hello true
true false
out_of_range
```

## See also

- [as_slice, operator slice_type](as_slice.md): a part of the characters as a slice, no copy
- [split](split.md): the pieces between the occurrences of a separator
- [sgcl::string](../string.md)
