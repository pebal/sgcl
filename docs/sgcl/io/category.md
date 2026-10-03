[sgcl](../README.md) › [io](README.md)

# sgcl::io::category

```cpp
#include "sgcl/io/error.h"   // or "sgcl/io.h"

namespace sgcl::io {
    const std::error_category& category() noexcept;
}
```

Returns the error category of the module, the one an [errc](errc.md) belongs to, named `"io"`: the counterpart of
`std::system_category()` for the failures no `errno` names. Its `message` gives the text of each code, and
`"unknown io error"` for a number that is none. One object, made at the first call.

## Parameters

None.

## Return value

The category, the same object at every call.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    const std::error_category& io_errors = io::category();
    println("{}", io_errors.name());
    println("{}", io_errors.message(int(io::errc::closed)));

    error_code code = io::open("missing.txt").error().code();
    println("{}", code.category() == io_errors);
}
```

Output:

```text
io
stream closed
false
```

## See also

- [errc](errc.md), [make_error_code](make_error_code.md)
- [error](error.md)
