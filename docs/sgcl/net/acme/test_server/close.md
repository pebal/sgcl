[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [test_server](README.md)

# sgcl::net::acme::test_server::close

```cpp
void close() const;
```

The server stopped: its listener and its connections closed. What its destructor does; a second close does nothing.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the connections open.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    ca.close();
    println("{}", acme.directory().error().code() == std::errc::connection_refused);
}
```

Output:

```text
true
```

## See also

- [sgcl::net::acme::test_server](README.md)
