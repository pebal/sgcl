[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [address](README.md)

# sgcl::net::imap::address::email

```cpp
string email() const noexcept;
```

Returns the address as written in a header: `mailbox@host`, or the mailbox alone when the host is empty.

## Parameters

None.

## Return value

The address, a new string; the mailbox's own when there is no host.

## Complexity

Linear in the size of the address.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::address a{"Bob", "bob", "example.com"};
    println("{}", a.email());
    net::imap::address local{"", "root", ""};
    println("{}", local.email());
}
```

Output:

```text
bob@example.com
root
```

## See also

- [address](README.md)
