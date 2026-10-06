[sgcl](../../README.md) › [txt](../README.md) › [script_code](README.md)

# sgcl::txt::script_code::code

```cpp
string code() const;
```

Returns the code: `"Latn"`.

## Parameters

None.

## Return value

The code; an empty text for none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    println("[{}] [{}]", txt::script_code("latn").code(), txt::script_code().code());
}
```

Output:

```text
[Latn] []
```

## See also

- [display_name](display_name.md)
- [sgcl::txt::script_code](README.md)
