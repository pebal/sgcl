[sgcl](../../README.md) › [txt](../README.md) › [value](README.md)

# sgcl::txt::value::truthy

```cpp
bool truthy() const noexcept;
```

What an `if` and a `with` of a template ask, and what the function `default` calls empty. The rule is the one a
reader of Go or of Python expects: nothing, `false`, a number that is nought, text with no characters and a list or a
mapping with no elements are all false, and everything else is true.

## Parameters

None.

## Return value

`true` when the value is true.

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
    txt::value values[] = {nullptr, false, 0, 0.0, "", txt::list{1}, "no", 7, -0.5};
    for (auto& v : values) {
        print("{} ", v.truthy());
    }
    println("");
    return 0;
}
```

Output:

```text
false false false false false true true true true 
```

## See also

- [stencil](../stencil/README.md#the-syntax): `if`, `with` and `range`
- [sgcl::txt::value](README.md)
