[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [form](README.md)

# sgcl::net::http::form::form

```cpp
form() noexcept;                                     // (1)
form(std::initializer_list<part> parts) noexcept;    // (2)
form(const form&) = default;                         // (3), implicitly declared
```

1. A form of no part. Sent, its body is the close delimiter alone.
2. A form of `parts`, in their order: a field written `{"name", "value"}`, a file [file](file.md)`("name", "path")`.
3. The same form: a form is a handle, and the copy shares its parts and its boundary.

- (1–2) The boundary is drawn now, 16 random bytes in hexadecimal.

## Parameters

| Parameter | Description |
|---|---|
| `parts` | the fields and the files, in their order |

## Complexity

Linear in the number of parts.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::form empty;
    net::http::form two{{"a", "1"}, {"b", "2"}};
    net::http::form same = two;
    same.add("c", "3");
    println("{} {}", empty.boundary().size(), empty.boundary() == two.boundary());
    println("{}", two.content_length().value() == same.content_length().value());
}
```

Output:

```text
32 false
true
```

## See also

- [add](add.md): a part after the others
- [form](README.md)
