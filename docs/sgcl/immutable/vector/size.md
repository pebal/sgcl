[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::size

```cpp
size_type size() const noexcept;
```

Returns the number of elements, a word of the vector.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v = {1, 2, 3};
    auto w = v.push_back(4).push_back(5);
    println("{} {}", v.size(), w.size());
}
```

Output:

```text
3 5
```

## See also

- [empty](empty.md): checks whether the vector is empty
- [depth](depth.md): the levels of the trie
- [sgcl::immutable::vector\<T\>](README.md)
