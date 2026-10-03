[sgcl](../../README.md) › [async](../README.md) › [wait_group](../wait_group.md)

# sgcl::async::operator==, operator!= (sgcl::async::wait_group)

```cpp
friend bool operator==(const wait_group& a, const wait_group& b) noexcept;
```

Checks whether two handles stand for the same group: `true` when they share the state, that is when one is a copy
of the other or both are copies of one. Two groups made apart are never equal, whatever their counts. The `!=` is
the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same group, `false` otherwise.

## Complexity

Constant: two words compared.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::wait_group a;
    async::wait_group b = a;
    async::wait_group c;
    println("{} {} {}", a == b, a == c, a != c);
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](wait_group.md): a handle of the same group
- [sgcl::async::wait_group](../wait_group.md)
