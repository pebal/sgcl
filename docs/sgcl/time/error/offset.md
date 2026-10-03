[sgcl](../../README.md) › [time](../README.md) › [error](../error.md)

# sgcl::time::error::offset

```cpp
size_t offset() const noexcept;
```

The byte of the input the reading stopped on: where the field that failed starts, in the text read or in the
bytes of the file. It is 0 where the whole input is refused, a name of a time zone the system does not have.

## Parameters

None.

## Return value

The offset from the start of the input, in bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    string text = "24.09.2026 25:00";
    auto t = time::datetime::parse(text, "%d.%m.%Y %H:%M");
    size_t at = t.error().offset();
    println("{}", t.error().message());
    println("{}", text);
    println("{}^", string(at, ' '));
}
```

Output:

```text
an hour from 0 to 23 expected
24.09.2026 25:00
           ^
```

## See also

- [message](message.md): why the reading stopped
- [sgcl::time::error](../error.md)
