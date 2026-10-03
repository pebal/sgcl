[sgcl](../../README.md) › [slog](../README.md) › [record](../record.md)

# sgcl::slog::record::message

```cpp
slice<const char> message() const noexcept;
```

Returns the text of the record's [message](../message.md): a view of the caller's text in the record a handler is
given, of the clone's own copy in a [clone](clone.md).

## Parameters

None.

## Return value

A slice of the text.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    {
        string text = "made on the fly";
        slog::logger(kept).info(text);
    }
    println("{}", kept.records()[0].message());
}
```

Output:

```text
made on the fly
```

## See also

- [message::text](../message/text.md)
- [sgcl::slog::record](../record.md)
