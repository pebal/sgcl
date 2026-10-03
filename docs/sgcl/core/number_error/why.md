[sgcl](../../README.md) › [core](../README.md) › [number_error](../number_error.md)

# sgcl::number_error::why

```cpp
constexpr reason why() const noexcept;
```

Returns why the text is not a number of the type: `empty`, `not_a_number`, `trailing` or `out_of_range`
([reason](../number_error-reason.md)).

## Parameters

None.

## Return value

The reason.

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
    for (string text : {"7", "7 ", "seven", "70000"}) {
        auto n = parse<int16_t>(text);
        if (n) {
            println("{}", *n);
        } else if (n.error().why() == number_error::reason::out_of_range) {
            println("too large for 16 bits");
        } else {
            println("not a number: {}", n.error().message());
        }
    }
}
```

Output:

```text
7
not a number: more after the number
not a number: not a number
too large for 16 bits
```

## See also

- [offset](offset.md): where the reading stopped
- [code](code.md), [message](message.md): the reason as a `std::errc`, as a text
- [sgcl::number_error](../number_error.md)
