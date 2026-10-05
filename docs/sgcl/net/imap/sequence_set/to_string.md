[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::to_string

```cpp
string to_string() const noexcept;
```

Returns the set as IMAP writes it: `1:4,7,10:*`, `$`.

## Parameters

None.

## Return value

The text; empty for an empty set.

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
    println("{}", net::imap::sequence_set(4, 2).to_string());
}
```

Output:

```text
2:4
```

## See also

- [parse](parse.md)
- [sequence_set](README.md)
