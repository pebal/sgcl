[sgcl](../README.md) › [core](README.md) › [number_error](number_error.md)

# sgcl::number_error::reason

```cpp
#include "sgcl/core/string.h"   // or "sgcl/core.h"

namespace sgcl {
    class number_error {
    public:
        enum class reason : uint8_t {
            empty,
            not_a_number,
            trailing,
            out_of_range
        };
    };
}
```

Why [parse](parse.md) read no number from a text, as [why](number_error/why.md) returns it. The offset the error
carries with it is given for each.

| Value | Description |
|---|---|
| `empty` | the text is empty; the offset is 0 |
| `not_a_number` | the text does not begin as a number of the type, or the base is outside 2 to 36; the offset is 0 |
| `trailing` | a number, and more after it; the offset is the first byte after the number |
| `out_of_range` | a number the type cannot hold; the offset is the first byte after the number |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto n = parse<int8_t>("200");
    println("{}", n.error().why() == number_error::reason::out_of_range);
    println("{}", parse<bool>("yes").error().why() == number_error::reason::not_a_number);
}
```

Output:

```text
true
true
```

## See also

- [number_error](number_error.md)
- [parse](parse.md)
