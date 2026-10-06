[sgcl](../README.md) › [io](README.md)

# sgcl::io::lock_mode

```cpp
#include "sgcl/io/lock.h"   // or "sgcl/io.h"

namespace sgcl::io {
    enum class lock_mode { shared, exclusive };
}
```

`sgcl::io::lock_mode` is how a lock of [lock_file](lock_file.md) holds a file: shared by any number of holders at
once, readers, or exclusive, one holder, a writer.

| Value | Description |
|---|---|
| `shared` | any number of shared locks at once; none while an exclusive one is held |
| `exclusive` | one lock, none other beside it |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("mode.lock", {.mode = io::lock_mode::shared}).value();
    println("{}", held.mode() == io::lock_mode::shared);
}
```

Output:

```text
true
```

## See also

- [lock_options](lock_options.md), [lock_file](lock_file.md)
