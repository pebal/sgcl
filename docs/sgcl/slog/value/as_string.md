[sgcl](../../README.md) › [slog](../README.md) › [value](../value.md)

# sgcl::slog::value::as_string

```cpp
string as_string() const;
```

Returns the text of a value of kind `string`, whatever made it: a literal, a `string`, an `io::error`'s or an `error_code`'s message, an exception's text, or the text of a type that writes itself (`write_text`, `format_value`, `to_text`, `to_string`), written now. A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The text, a new [string](../../core/string.md).

## Complexity

Linear in the length of the text.

## Exceptions

`logic_error` when [type](type.md) is not `string`: `sgcl::slog::value::as_string: a value of another kind`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    auto peer = net::endpoint(net::ip_address::v4(10, 0, 0, 2), 443);
    slog::logger(kept).info("m", "name", "ala", "peer", peer);
    for (auto a : kept.records()[0]) {
        println("{} {}", a.key(), a.value().as_string());
    }
}
```

Output:

```text
name ala
peer 10.0.0.2:443
```

## See also

- [text](text.md)
- [kind](../value-kind.md): `string`
- [sgcl::slog::value](../value.md)
