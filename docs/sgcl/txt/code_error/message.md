[sgcl](../../README.md) › [txt](../README.md) › [code_error](README.md)

# sgcl::txt::code_error::message

```cpp
string message() const;
```

Returns the error as a sentence: what was expected.

## Parameters

None.

## Return value

The sentence.

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
    println("{}", txt::currency::parse("12").error().message());
}
```

Output:

```text
not a currency code: three ASCII letters expected
```

## See also

- [offset](offset.md)
- [sgcl::txt::code_error](README.md)
