[sgcl](../../README.md) › [io](../README.md) › [reader](../reader.md)

# sgcl::io::reader::has_close

```cpp
bool has_close() const noexcept;
```

Checks whether the stream has a close of its own, `close` or `async_close` ([req::closer](../req/closer.md),
[req::async_closer](../req/closer.md)): whether [close](close.md) through the reader closes anything.

## Parameters

None.

## Return value

`true` when the stream has `close` or `async_close`; `false` when it has neither, and for an empty reader.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

struct Source {
    expected<size_t, io::error> read(const slice<byte>&) {
        return 0;
    }

    expected<void, io::error> close() {
        return {};
    }
};

int main() {
    Source source;
    io::reader closable = source;
    io::reader memory = io::buffer("nothing to close");
    println("{} {} {}", closable.has_close(), memory.has_close(), io::reader().has_close());
}
```

Output:

```text
true false false
```

## See also

- [close, async_close](close.md)
- [sgcl::io::reader](../reader.md)
