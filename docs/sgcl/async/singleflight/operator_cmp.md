[sgcl](../../README.md) › [async](../README.md) › [singleflight](README.md)

# sgcl::async::operator==, operator!= (sgcl::async::singleflight)

```cpp
friend bool operator==(const singleflight& a, const singleflight& b) noexcept;
```

Checks whether two handles stand for the same table: `true` when one is a copy of the other or both are copies of
one. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same table, `false` otherwise.

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
    async::singleflight a;
    async::singleflight b = a;
    async::singleflight c;
    println("{} {} {}", a == b, a == c, a != c);
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](singleflight.md): a handle of the same table
- [sgcl::async::singleflight](README.md)
