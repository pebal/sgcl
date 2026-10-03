[sgcl](../../README.md) › [encoding](../README.md) › [error](README.md)

# sgcl::encoding::error::io_error

```cpp
const optional<io::error>& io_error() const noexcept;
```

The error of the stream when the input came from one, or went into one, and that failed: a file that did not open, a
read or a write that failed. The error's [code](code.md) is then `errc::io`, and [message()](message.md) ends with
the stream's message; the error of a file of `load` or `save` has no place before it.

## Parameters

None.

## Return value

The stream's [io::error](../../io/error/README.md), or an empty optional when the error is the input's.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x = 0;
    int y = 0;

    void describe(encoding::field_list& f) {
        f.add("x", x);
        f.add("y", y);
    }
};

int main() {
    auto points = encoding::csv::load<point>("no-such-file.csv");
    const encoding::error& e = points.error();
    println("{}", e.code() == encoding::errc::io);
    println("{}", e.io_error()->message());
    println("{}", e.message());
}
```

Output:

```text
true
open no-such-file.csv: No such file or directory
input/output error: open no-such-file.csv: No such file or directory
```

## See also

- [io::error](../../io/error/README.md): the stream's error
- [(constructor)](error.md): an error of a stream's error
- [sgcl::encoding::error](README.md)
