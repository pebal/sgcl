[sgcl](../../README.md) › [codec](../README.md) › [error](../error.md)

# sgcl::codec::error::io_error

```cpp
const optional<io::error>& io_error() const noexcept;
```

The error of the stream or the file, when the code is `errc::io`: the [io::error](../../io/error.md) the source or the
sink failed with, its operation, its path and its code as io gave them. [load](../load.md) of a file that does not
read, [save](../save.md) of a file that cannot be created or renamed, a decoder whose stream fails and an encoder
whose stream does not take the bytes report it here. Every other code has none.

## Parameters

None.

## Return value

A reference to the stream's error, or to an empty `optional`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/codec.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto photo = codec::load("no-such-photo.png");
    const codec::error& e = photo.error();
    println("{}", e.code() == codec::errc::io);
    if (e.io_error()) {
        println("{} {}: not found {}", e.io_error()->op(), e.io_error()->path(),
                e.io_error()->is_not_found());
    }
    vector<byte> nothing(4);
    println("{}", codec::decode(nothing).error().io_error().has_value());
}
```

Output:

```text
true
open no-such-photo.png: not found true
false
```

## See also

- [io::error](../../io/error.md): the error of a stream
- [code](code.md): `errc::io`
- [sgcl::codec::error](../error.md)
