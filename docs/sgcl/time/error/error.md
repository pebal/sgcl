[sgcl](../../README.md) › [time](../README.md) › [error](README.md)

# sgcl::time::error::error

```cpp
explicit error(const string& message, size_t offset = 0) noexcept;
```

An error of the sentence `message` and the byte `offset`. The library makes the errors of its readings; a program
makes one for a reading of its own that reports as the module does, in the same `expected`.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the sentence: why the input is not what it should be |
| `offset` | the byte of the input the reading stopped on |

## Complexity

Constant: the sentence is shared, not copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

expected<time::date, time::error> parse_day(const string& text) {
    if (text.size() != 10) {
        return unexpected(time::error("a date of ten characters expected", text.size()));
    }
    return time::date::parse(text);
}

int main() {
    auto bad = parse_day("2026-9-24");
    println("{} (byte {})", bad.error().message(), bad.error().offset());
    println("{}", parse_day("2026-09-24").value());
}
```

Output:

```text
a date of ten characters expected (byte 9)
2026-09-24
```

## See also

- [message](message.md), [offset](offset.md): what the error says
- [sgcl::time::error](README.md)
