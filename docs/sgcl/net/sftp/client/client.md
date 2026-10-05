[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [client](README.md)

# sgcl::net::sftp::client::client

```cpp
client() noexcept;
```

No session: `false` as a bool. A client is made by [connect](connect.md); this one is only a place to assign one to.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/sftp.h"

using namespace sgcl;

int main() {
    net::sftp::client fs;
    println("{}", bool(fs));
}
```

Output:

```text
false
```

## See also

- [connect](connect.md)
- [operator bool](operator_bool.md)
- [sgcl::net::sftp::client](README.md)
