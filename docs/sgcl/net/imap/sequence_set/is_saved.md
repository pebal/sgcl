[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::is_saved

```cpp
bool is_saved() const noexcept;
```

Returns whether the set is `$`, the result a search saved.

## Parameters

None.

## Return value

`true` for `$`.

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
    println("{} {}", net::imap::sequence_set("$").is_saved(), net::imap::sequence_set(1).is_saved());
}
```

Output:

```text
true false
```

## See also

- [saved](saved.md)
- [sequence_set](README.md)
