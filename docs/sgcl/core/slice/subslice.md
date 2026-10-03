[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::subslice

```cpp
slice subslice(size_type pos, size_type n = npos) const;
```

The elements `[pos, pos + n)` as a slice of the same owner, `n` cut to what is left after `pos`: `subslice(pos)` is
every element from `pos` on. Nothing is copied; the piece holds the owner as the slice does.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the first element of the piece |
| `n` | the number of elements, at most `size() - pos`; `npos` for all of them |

## Return value

A slice of the elements, with the same owner.

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
    vector v = {1, 2, 3, 4, 5};
    slice<int> s = v;
    println("{} {} {}", s.subslice(1, 3), s.subslice(3), s.subslice(1, 100));
    try {
        s.subslice(6);
    } catch (const out_of_range&) {
        println("past the end");
    }
}
```

Output:

```text
[2, 3, 4] [4, 5] [2, 3, 4, 5]
past the end
```

## See also

- [first](first.md), [last](last.md): the first, the last `n` elements
- [subspan](subspan.md), [substr](substr.md): the same under `std`'s names
- [sgcl::slice\<T\>](README.md)
