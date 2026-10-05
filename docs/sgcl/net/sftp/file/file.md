[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md) › [file](README.md)

# sgcl::net::sftp::file::file

```cpp
file() noexcept;
```

No file: `false` as a bool. A file is opened by [client::open](../client/open.md) or [client::create](../client/create.md); this one is only a place to assign one to.

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
    net::sftp::file f;
    println("{}", bool(f));
}
```

Output:

```text
false
```

## See also

- [client::open](../client/open.md)
- [operator bool](operator_bool.md)
- [sgcl::net::sftp::file](README.md)
