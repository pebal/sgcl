[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::operator==, operator!= (sgcl::net::imap::sequence_set)

```cpp
friend bool operator==(const sequence_set& a, const sequence_set& b) noexcept;
```

Returns whether two sets have the same ranges in the same order (`1,2` and `1:2` are not equal: the sets are compared
as written). `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the sets |

## Return value

`true` when equal.

## Complexity

Linear in the number of ranges.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{}", net::imap::sequence_set("1:3") == net::imap::sequence_set(1, 3));
    println("{}", net::imap::sequence_set("1,2") == net::imap::sequence_set(1, 2));
}
```

Output:

```text
true
false
```

## See also

- [sequence_set](README.md)
