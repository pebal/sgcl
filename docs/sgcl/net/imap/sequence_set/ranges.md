[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::ranges

```cpp
vector<pair<uint32_t, uint32_t>> ranges() const noexcept;
```

Returns the ranges as pairs, in their order, `net::imap::last` (0) for `*`; a single number is a range of one.

## Parameters

None.

## Return value

The pairs.

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
    for (auto [first, last] : net::imap::sequence_set("2,5:7,9:*").ranges()) {
        println("{} {}", first, last);
    }
}
```

Output:

```text
2 2
5 7
9 0
```

## See also

- [sequence_set](README.md)
