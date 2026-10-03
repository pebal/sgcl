[sgcl](../../README.md) › [encoding](../README.md) › [error](../error.md)

# sgcl::encoding::error::set_path

```cpp
error& set_path(const string& path) noexcept;
```

Sets the [path](path.md), where in the structure the error was found: a JSON Pointer, `/users/3/age`, for a format of
objects and arrays, a path of elements for XML. [message()](message.md) shows it after the position.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path; empty for none |

## Return value

`*this`.

## Complexity

Constant: the string is shared, not copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "{\"order\": {\n  \"items\": [1, 2,\n  {\"id\": 7}]\n}}";
    encoding::error e(encoding::errc::missing_field, 32);
    e.set_path("/order/items/2/price").locate(text);
    println("{}", e.message());
}
```

Output:

```text
3:3 /order/items/2/price: missing field
```

## See also

- [path](path.md): the path
- [sgcl::encoding::error](../error.md)
