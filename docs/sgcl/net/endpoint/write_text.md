[sgcl](../../README.md) › [net](../README.md) › [endpoint](../endpoint.md)

# sgcl::net::endpoint::write_text

```cpp
size_t write_text(char* out) const noexcept;
```

Writes the text [to_string](to_string.md) gives into `out`, without a terminator, and makes no string: for a line made
without an allocation, such as a log line of [slog](../../slog/README.md) or a server's access log. `out` holds at
least `MaxText` bytes.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer written, of at least `MaxText` (63) bytes |

## Return value

The number of bytes written.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"
#include <string_view>

using namespace sgcl;

int main() {
    char text[net::endpoint::MaxText];
    size_t n = net::endpoint("[::1]:8080").write_text(text);
    println("{} {}", n, std::string_view(text, n));
}
```

Output:

```text
10 [::1]:8080
```

## See also

- [to_string](to_string.md): the text as a string
- [sgcl::net::endpoint](../endpoint.md)
