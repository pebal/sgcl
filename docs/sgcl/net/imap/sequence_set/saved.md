[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::saved

```cpp
static sequence_set saved() noexcept;
```

Returns `$`: the result a search saved (SEARCHRES, RFC 5182), which a command sent after the search takes without the
numbers coming back first.

## Parameters

None.

## Return value

The set.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::sequence_set s = net::imap::sequence_set::saved();
    println("{} {}", s.to_string(), s.is_saved());
}
```

Output:

```text
$ true
```

## See also

- [is_saved](is_saved.md)
- [sequence_set](README.md)
