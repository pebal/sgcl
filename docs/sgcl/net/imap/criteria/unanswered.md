[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::unanswered

```cpp
static criteria unanswered() noexcept;
```

Returns the search key of the messages without `\Answered`: IMAP's `UNANSWERED`.

## Parameters

None.

## Return value

The criteria.

## Complexity

Linear in the size of the arguments.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{}", net::imap::criteria::unanswered().to_string());
}
```

Output:

```text
UNANSWERED
```

## See also

- [search](../client/search.md)
- [sgcl::net::imap::criteria](README.md)
