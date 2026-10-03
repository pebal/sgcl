[sgcl](../../README.md) › [core](../README.md) › [duration_error](README.md)

# sgcl::duration_error::duration_error

```cpp
constexpr duration_error(const char* reason, size_t offset) noexcept;
```

An error of the sentence `reason` and the byte `offset`. The sentence is not copied: it is a literal, kept by its
pointer, so `reason` outlives the error. [duration::parse](../duration/parse.md) makes the errors; a program makes
one for a reading of its own that reports as `parse` does.

## Parameters

| Parameter | Description |
|---|---|
| `reason` | the sentence, a string literal |
| `offset` | the byte of the text the reading stopped on |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

expected<duration, duration_error> parse_minutes(const string& text) {
    if (text.empty() || text.back() != 'm') {
        return unexpected(duration_error("minutes expected", text.size()));
    }
    return duration::parse(text);
}

int main() {
    auto bad = parse_minutes("90s");
    println("{} (byte {})", bad.error().message(), bad.error().offset());
    println("{}", parse_minutes("90m").value());
}
```

Output:

```text
minutes expected (byte 3)
1h30m0s
```

## See also

- [message](message.md), [offset](offset.md): what the error says
- [sgcl::duration_error](README.md)
