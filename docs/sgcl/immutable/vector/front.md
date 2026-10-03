[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::front

```cpp
const_reference front() const noexcept;
```

Returns a reference to the first element: the first element of the trie's first leaf, or of the tail while the
vector holds 32 elements or fewer.

## Parameters

None.

## Return value

A `const` reference to the first element.

## Complexity

Logarithmic in `size()`, base 32: `depth()` branches walked to the first leaf.

## Exceptions

None. `front` on an empty vector is undefined; debug builds assert.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<string> names = {"Ada", "Grace", "Linus"};
    auto renamed = names.set(0, "Barbara");
    println("{} {}", names.front(), renamed.front());
}
```

Output:

```text
Ada Barbara
```

## See also

- [back](back.md): the last element
- [operator[]](operator_at.md): the element at a position
- [sgcl::immutable::vector\<T\>](README.md)
