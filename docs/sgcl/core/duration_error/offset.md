[sgcl](../../README.md) › [core](../README.md) › [duration_error](README.md)

# sgcl::duration_error::offset

```cpp
size_t offset() const noexcept;
```

The byte of the text the reading stopped on: the start of the number, of the unit or of the place a unit was
expected, counted from the first byte of the text.

## Parameters

None.

## Return value

The offset in bytes.

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
    string text = "1h30x";
    auto d = duration::parse(text);
    size_t at = d.error().offset();
    println("{}", text);
    println("{}^ {}", string(at, ' '), d.error().message());
}
```

Output:

```text
1h30x
    ^ an unknown unit: ns, us, ms, s, m or h expected
```

## See also

- [message](message.md): the sentence
- [sgcl::duration_error](README.md)
