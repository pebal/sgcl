[sgcl](../../README.md) › [immutable](../README.md) › [vector](README.md)

# sgcl::immutable::vector\<T\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the vector has no elements. An empty vector holds no node at all.

## Parameters

None.

## Return value

`true` when `size() == 0`, `false` otherwise.

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
    immutable::vector<int> v;
    auto one = v.push_back(1);
    println("{} {} {}", v.empty(), one.empty(), one.pop_back().empty());
}
```

Output:

```text
true false true
```

## See also

- [size](size.md): the number of elements
- [sgcl::immutable::vector\<T\>](README.md)
