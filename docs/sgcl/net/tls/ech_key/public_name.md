[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ech_key](README.md)

# sgcl::net::tls::ech_key::public_name

```cpp
string public_name() const;
```

The public name: what the outer hellos sealed to the key name in their SNI, and what a rejected client checks the server for.

## Parameters

None.

## Return value

The name.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    println("{}", net::tls::ech_key::generate("public.example").public_name());
}
```

Output:

```text
public.example
```

## See also

- [generate](generate.md)
- [sgcl::net::tls::ech_key](README.md)
