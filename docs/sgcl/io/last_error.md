[sgcl](../README.md) › [io](README.md)

# sgcl::io::last_error

```cpp
#include "sgcl/io/error.h"   // or "sgcl/io.h"

namespace sgcl::io {
    error last_error(const string& op, const string& path = {}) noexcept;
}
```

Returns the [error](error.md) of `errno` after a system call failed: `error(error_code(errno, std::system_category()),
op, path)`. What a stream or a function of the program's returns when a call of its own fails, in the form of the
module's errors, the predicates included. It is read at once, before another call sets `errno` again.

## Parameters

| Parameter | Description |
|---|---|
| `op` | the operation that failed |
| `path` | the path or the name of the stream it was on; none by default |

## Return value

The error of the code `errno` holds, in the system category.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include <unistd.h>

using namespace sgcl;

expected<void, io::error> remove_lock(const string& path) {
    if (::unlink(path.c_str()) != 0) {
        return unexpected(io::last_error("unlink", path));
    }
    return {};
}

int main() {
    auto r = remove_lock("no.lock");
    println("{}: not found? {}", r.error().message(), r.error().is_not_found());
}
```

Output:

```text
unlink no.lock: No such file or directory: not found? true
```

## See also

- [error](error.md)
- [errc](errc.md): the failures no `errno` names
