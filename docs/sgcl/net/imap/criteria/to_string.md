[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [criteria](README.md)

# sgcl::net::imap::criteria::to_string

```cpp
string to_string() const noexcept;
```

Returns the search keys as the command writes them, strings quoted: what is sent after `UID SEARCH` (a connection may
write a string as a literal instead).

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{}", (net::imap::criteria::subject("say \"hi\"") && net::imap::criteria::seen()).to_string());
}
```

Output:

```text
SUBJECT "say \"hi\"" SEEN
```

## See also

- [sgcl::net::imap::criteria](README.md)
