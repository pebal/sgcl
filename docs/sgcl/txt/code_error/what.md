[sgcl](../../README.md) › [txt](../README.md) › [code_error](README.md)

# sgcl::txt::code_error::what

```cpp
constexpr kind what() const noexcept;
```

Returns the kind of code the text was read as: `kind::currency` (three ASCII letters), `kind::region` (two ASCII
letters or three digits), `kind::script` (four ASCII letters).

## Parameters

None.

## Return value

The kind.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::currency::parse("PL").error().what() == txt::code_error::kind::currency);
}
```

Output:

```text
true
```

## See also

- [message](message.md)
- [sgcl::txt::code_error](README.md)
