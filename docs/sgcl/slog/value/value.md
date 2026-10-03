[sgcl](../../README.md) › [slog](../README.md) › [value](README.md)

# sgcl::slog::value::value

```cpp
value() noexcept = default;
```

Constructs a value of kind `null`, what an absent value reads as: `<nil>` in [text](text.md), `null` in [json](json.md). The values a handler reads are made by the module, views of a record.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::value v;
    println("{} {} {}", v.type() == slog::value::kind::null, v.text(), v.json());
}
```

Output:

```text
true <nil> null
```

## See also

- [type](type.md)
- [sgcl::slog::value](README.md)
