[sgcl](../../README.md) › [compress](../README.md) › [error](README.md)

# sgcl::compress::error::io_error

```cpp
const optional<io::error>& io_error() const noexcept;
```

Returns the error of the stream underneath when the source or the sink failed (the code is then `errc::io`), and
`nullopt` for a failure of the data itself.

## Parameters

None.

## Return value

The stream's error, or `nullopt`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto opened = compress::zip::archive::open("no-such-file.zip");
    if (!opened && opened.error().io_error()) {
        const io::error& e = *opened.error().io_error();
        println("{}", e.message());
        println("{}", e.is_not_found());
    }
}
```

Output:

```text
open no-such-file.zip: No such file or directory
true
```

## See also

- [io::error](../../io/error/README.md): the error of the streams
- [sgcl::compress::error](README.md)
