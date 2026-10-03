[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::depth

```cpp
unsigned depth() const noexcept;
```

Returns the height of the trie: the number of levels of branches a random access walks before it reaches a leaf.
It is 0 while every element is in the tail (32 elements or fewer), 1 up to 1 056 elements, 2 up to 32 800, and one
more for every further factor of 32. The last 32 elements are in the tail at any height and are read without a
walk.

`depth` has no counterpart in `std::vector`: it is there for the curious and for the tests, and it says what a
random access costs.

## Parameters

None.

## Return value

The number of levels of branches above the leaves.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v;
    for (int n : {32, 33, 1056, 1057, 100000}) {
        while (v.size() < size_t(n)) {
            v = v.push_back(int(v.size()));
        }
        println("{} elements: depth {}", v.size(), v.depth());
    }
}
```

Output:

```text
32 elements: depth 0
33 elements: depth 1
1056 elements: depth 1
1057 elements: depth 2
100000 elements: depth 3
```

## See also

- [operator[]](operator_at.md): the element at a position, `depth()` branches away
- [size](size.md): the number of elements
- [sgcl::immutable::vector\<T\>](README.md)
