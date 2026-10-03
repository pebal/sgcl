[sgcl](../README.md) › [io](README.md)

# sgcl::io::pid

```cpp
#include "sgcl/io/os.h"   // or "sgcl/io.h"

namespace sgcl::io {
    int pid() noexcept;
}
```

Returns the id of the process, Go's `os.Getpid`, the C library's `getpid`.

## Parameters

None.

## Return value

The process id.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("process {}", io::pid());
}
```

Sample output:

```text
process 48213
```

## See also

- [process](process/README.md): a child process, its id among the rest
- [hostname](hostname.md): the name of the host
