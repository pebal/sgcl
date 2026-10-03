[sgcl](../../README.md) › [async](../README.md) › [event](../event.md)

# sgcl::async::operator==, operator!= (sgcl::async::event)

```cpp
friend bool operator==(const event& a, const event& b) noexcept;
```

Checks whether two handles stand for the same event: `true` when they share the channel, that is when one is a copy
of the other or both are copies of one. Two events made apart are never equal, set or not. The `!=` is the one C++
writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same event, `false` otherwise.

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
    async::event a;
    async::event b = a;
    async::event c;
    a.set();
    c.set();
    println("{} {} {}", a == b, a == c, a != c);
}
```

Output:

```text
true false true
```

## See also

- [(constructor)](event.md): a handle of the same event
- [sgcl::async::event](../event.md)
