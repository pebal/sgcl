[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::substr

```cpp
slice substr(size_type pos = 0, size_type n = npos) const;
```

[subslice](subslice.md) under `std::string_view`'s name, for a text slice: the characters `[pos, pos + n)` as a
slice of the same owner, `n` cut to what is left after `pos`. Unlike `std::string_view::substr`, which copies
nothing either, the piece keeps the text alive. Takes part only for a slice of a character type.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the first character |
| `n` | the number of characters, at most `size() - pos`; `npos` for all of them |

## Return value

A slice of the characters, with the same owner.

## Complexity

Constant.

## Exceptions

`out_of_range` when `pos > size()`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string date = "2026-10-02";
    string_slice s = date;
    println("{} {} {}", s.substr(0, 4), s.substr(5, 2), s.substr(8));
}
```

Output:

```text
2026 10 02
```

## See also

- [subslice](subslice.md): the same for any slice
- [sgcl::slice\<T\>](README.md)
