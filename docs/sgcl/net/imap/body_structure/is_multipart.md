[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [body_structure](README.md)

# sgcl::net::imap::body_structure::is_multipart

```cpp
bool is_multipart() const noexcept;
```

Returns whether the part is a multipart (its `type` is `multipart`), its parts in `parts`. A message/rfc822 part has
its message in `parts` too, but is not a multipart.

## Parameters

None.

## Return value

`true` for a multipart.

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
    net::imap::body_structure mixed;
    mixed.type = "multipart";
    mixed.subtype = "mixed";
    net::imap::body_structure text;
    text.type = "text";
    println("{} {}", mixed.is_multipart(), text.is_multipart());
}
```

Output:

```text
true false
```

## See also

- [body_structure](README.md)
