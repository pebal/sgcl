[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ech_key](README.md)

# sgcl::net::tls::ech_key::config_id

```cpp
uint8_t config_id() const noexcept;
```

The config's id, which the outer hellos sealed to the key name.

## Parameters

None.

## Return value

The id.

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
    println("{}", net::tls::ech_key::generate("public.example", {.config_id = 42}).config_id());
}
```

Output:

```text
42
```

## See also

- [options](../ech_key-options.md)
- [sgcl::net::tls::ech_key](README.md)
