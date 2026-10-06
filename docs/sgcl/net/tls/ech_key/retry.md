[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ech_key](README.md)

# sgcl::net::tls::ech_key::retry

```cpp
bool retry() const noexcept;
```

Whether the config is sent in the `retry_configs` of a rejected hello.

## Parameters

None.

## Return value

`true` when it is sent.

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
    auto key = net::tls::ech_key::generate("public.example", {.retry = false});
    println("{}", key.retry());
}
```

Output:

```text
false
```

## See also

- [options](../ech_key-options.md)
- [sgcl::net::tls::ech_key](README.md)
