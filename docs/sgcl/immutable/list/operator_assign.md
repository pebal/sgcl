[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::operator=

```cpp
list& operator=(const list& other) noexcept;    // (1)
list& operator=(list&& other) noexcept;         // (2)
```

Makes this variable hold the version `other` holds.

1. Copies the two words of `other`; every cell is shared.
2. The same as (1): `other` keeps its version.

The version the variable held before is not changed; its cells are left to the collector once no other list
reaches them. The assignment is how a program moves on to the next version, `l = l.push_front(x)`: the only
member that is not `const`, and the one thing that needs synchronization when other threads read the variable.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the list whose version is taken |

## Return value

`*this`.

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
    immutable::list<int> stack = {2, 3};
    immutable::list<int> before = stack;
    stack = stack.push_front(1);  // the variable moves on, the old version stays
    println("{} {}", stack, before);
}
```

Output:

```text
[1, 2, 3] [2, 3]
```

## See also

- [(constructor)](list.md): constructs the list
- [sgcl::immutable::list\<T\>](../list.md)
