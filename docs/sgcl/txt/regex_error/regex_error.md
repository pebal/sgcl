[sgcl](../../README.md) › [txt](../README.md) › [regex_error](README.md)

# sgcl::txt::regex_error::regex_error

```cpp
regex_error(const string& message, size_t offset) noexcept;
```

Constructs an error of a sentence and a byte of the pattern. [regex::compile](../regex/compile.md) makes the
errors of the library; the constructor is public for a program that refuses a pattern of its own by the same
shape, before or after the library has read it.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the sentence, as [message](message.md) returns it |
| `offset` | the byte of the pattern, as [offset](offset.md) returns it |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

expected<txt::regex, txt::regex_error> short_pattern(const string& p) {
    if (p.size() > 16) {
        return unexpected(txt::regex_error("the pattern is longer than 16 bytes", 16));
    }
    return txt::regex::compile(p);
}

int main() {
    auto r = short_pattern("[a-z]+@[a-z]+\\.[a-z]+");
    println("{} at {}", r.error().message(), r.error().offset());
}
```

Output:

```text
the pattern is longer than 16 bytes at 16
```

## See also

- [regex::compile](../regex/compile.md)
- [sgcl::txt::regex_error](README.md)
