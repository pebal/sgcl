[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [revocation_cache](README.md)

# sgcl::net::tls::revocation_cache::clear

```cpp
void clear() const noexcept;
```

Drops every entry of the cache: the next check of each certificate asks its responder, or fetches its CRL, again. A
program that learns of a revocation its cache does not know yet (a CA's announcement) clears it. The cache stays and
keeps what comes after.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::tls::revocation_cache answers;
    answers.clear();
    println("{}", answers.size());
}
```

Output:

```text
0
```

## See also

- [size](size.md): the entries held
- [sgcl::net::tls::revocation_cache](README.md)
