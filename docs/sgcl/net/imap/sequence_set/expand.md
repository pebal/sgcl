[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::expand

```cpp
vector<uint32_t> expand(uint32_t largest) const noexcept;
```

Returns the numbers of the set, `*` given the value `largest`, those past `largest` left out.

## Parameters

| Parameter | Description |
|---|---|
| `largest` | the last number in use |

## Return value

The numbers, ascending, without repeats.

## Complexity

O(n log n) in the numbers.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{}", net::imap::sequence_set("5,1:3,4:*").expand(6));
}
```

Output:

```text
[1, 2, 3, 4, 5, 6]
```

## See also

- [contains](contains.md)
- [sequence_set](README.md)
