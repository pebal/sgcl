[sgcl](../../README.md) › [slog](../README.md) › [value](README.md)

# sgcl::slog::value::as_int

```cpp
int64_t as_int() const;
```

Returns the value of a value of kind `int64`: the signed integer given, as `int64_t`. An unsigned integer is a `uint64`, read by [as_uint](as_uint.md). A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The signed integer given, as `int64_t`.

## Complexity

Constant.

## Exceptions

`logic_error` when [type](type.md) is not `int64`: `sgcl::slog::value::as_int: a value of another kind`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "delta", -5, "count", 5u);
    auto r = kept.records()[0];
    auto a = r.begin();
    println("{}", (*a).value().as_int());
    ++a;
    try {
        (*a).value().as_int();
    } catch (const std::logic_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
-5
sgcl::slog::value::as_int: a value of another kind
```

## See also

- [type](type.md)
- [kind](../value-kind.md): `int64`
- [sgcl::slog::value](README.md)
