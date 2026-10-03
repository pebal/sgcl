[sgcl](../README.md) › [io](README.md)

# sgcl::io::hostname

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    expected<string, error> hostname() noexcept;
}
```

Returns the name of the host, Go's `os.Hostname`, as the C library's `gethostname` gives it, up to 255 bytes.

## Parameters

None.

## Return value

The name, or the [error](error/README.md) of the call; the operation is `hostname`.

## Complexity

Constant: one call to the system.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    if (auto host = io::hostname()) {
        println("running on {}", *host);
    }
}
```

## See also

- [pid](pid.md): the id of the process
- [net::dns](../net/dns/README.md): the addresses of a name
