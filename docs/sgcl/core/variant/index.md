[sgcl](../../README.md) › [core](../README.md) › [variant](README.md)

# sgcl::variant\<Ts...\>::index

```cpp
size_t index() const noexcept;
```

The zero-based index of the alternative held, in the order of `Ts`; `variant_npos` when the variant is valueless.

## Parameters

None.

## Return value

The index of the alternative, or `variant_npos`.

## Complexity

Constant.

## Exceptions

None.

## Notes

The index is one byte of the variant for fewer than 255 alternatives.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Node {
    int value;
};

int main() {
    variant<monostate, int, tracked_ptr<Node>> v;
    println("{}", v.index());
    v = 7;
    println("{}", v.index());
    v = make_tracked<Node>(1);
    println("{}", v.index());
}
```

Output:

```text
0
1
2
```

## See also

- [holds_alternative](holds_alternative.md): checks for an alternative by its type
- [valueless_by_exception](valueless_by_exception.md): checks whether the variant holds nothing
- [sgcl::variant\<Ts...\>](README.md)
