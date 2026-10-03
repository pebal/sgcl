[sgcl](../../README.md) › [immutable](../README.md) › [vector](../vector.md)

# sgcl::immutable::vector\<T\>::operator=

```cpp
/*(1)*/ vector& operator=(const vector& other) noexcept;
/*(2)*/ vector& operator=(vector&& other) noexcept;
```

Makes this variable hold the version `other` holds.

1. Copies the four words of `other`; every node is shared.
2. The same as (1): `other` keeps its version.

The version the variable held before is not changed; its nodes are left to the collector once no other version
reaches them. The assignment is how a program moves on to the next version, `v = v.push_back(x)`: the only
member that is not `const`, and the one thing that needs synchronization when other threads read the variable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the vector whose version is taken |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Notes

An iterator taken from this vector before the assignment is not valid after it.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::vector<int> v = {1, 2};
    immutable::vector<int> before = v;
    v = v.push_back(3);  // the variable moves on, the old version stays
    println("{} {}", v, before);
}
```

Output:

```text
[1, 2, 3] [1, 2]
```

## See also

- [(constructor)](vector.md): constructs the vector
- [sgcl::immutable::vector\<T\>](../vector.md)
