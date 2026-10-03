[sgcl](../../README.md) › [core](../README.md) › [number_error](README.md)

# sgcl::number_error::message

```cpp
string message() const noexcept;
```

Returns the reason as a short text in English, for a person: `"an empty text"`, `"not a number"`, `"more after the
number"` or `"a number out of the type's range"`.

## Parameters

None.

## Return value

The text of the reason.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string port = "80a";
    auto n = parse<uint16_t>(port);
    if (!n) {
        println("port \"{}\": {} at byte {}", port, n.error().message(), n.error().offset());
    }
}
```

Output:

```text
port "80a": more after the number at byte 2
```

## See also

- [why](why.md): the reason as a value
- [sgcl::number_error](README.md)
