[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::subspan

```cpp
slice subspan(size_type pos, size_type n = npos) const;
```

[subslice](subslice.md) under `std::span`'s name: the elements `[pos, pos + n)` as a slice of the same owner, `n`
cut to what is left after `pos`. Unlike `std::span::subspan`, a count past the end is cut, not undefined, and a
`pos` past the end throws.

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
    vector v = {1, 2, 3, 4};
    slice<int> s = v;
    println("{}", s.subspan(2));
}
```

Output:

```text
[3, 4]
```

## See also

- [subslice](subslice.md): the same
- [sgcl::slice\<T\>](README.md)
