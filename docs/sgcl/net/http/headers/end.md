[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [headers](../headers.md)

# sgcl::net::http::headers::end

```cpp
iterator end() const noexcept;
```

Returns the iterator past the last field. It is not read; a loop stops at it.

## Parameters

None.

## Return value

The iterator past the last field.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::headers h;
    println("{}", h.begin() == h.end());
    h.set("Accept", "*/*");
    int fields = 0;
    for (auto it = h.begin(); it != h.end(); ++it) {
        ++fields;
    }
    println("{}", fields);
}
```

Output:

```text
true
1
```

## See also

- [begin](begin.md): the first field
- [sgcl::net::http::headers](../headers.md)
