[sgcl](../../README.md) › [io](../README.md) › [writer](../writer.md)

# sgcl::io::writer::has_close

```cpp
bool has_close() const noexcept;
```

Checks whether the stream has a close of its own, `close` or `async_close` ([req::closer](../req/closer.md),
[req::async_closer](../req/closer.md)): whether [close](close.md) through the writer closes anything. The
standard streams have none: `io::stdout` is never closed by a writer.

## Parameters

None.

## Return value

`true` when the stream has `close` or `async_close`; `false` when it has neither, and for an empty writer.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

struct Sink {
    expected<size_t, io::error> write(const slice<const byte>& data) {
        return data.size();
    }

    expected<void, io::error> close() {
        return {};
    }
};

int main() {
    Sink sink;
    io::writer closable = sink;
    io::writer terminal = io::stdout;
    println("{} {} {}", closable.has_close(), terminal.has_close(), io::writer().has_close());
}
```

Output:

```text
true false false
```

## See also

- [close, async_close](close.md)
- [sgcl::io::writer](../writer.md)
