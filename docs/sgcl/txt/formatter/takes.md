[sgcl](../../README.md) › [txt](../README.md) › [formatter](../formatter.md)

# sgcl::txt::formatter\<T\>::takes

```cpp
static constexpr bool takes(char type) noexcept;
```

Whether the type takes the type letter `type` — `'d'`, `'x'`, `'s'`, `'?'` — or `0`, none written. Asked where the
pattern is read: where the program is compiled for a literal, and where it runs for a
[runtime](../runtime.md) pattern, which answers `nullopt` instead. A specialization without it takes every
specification; one with it declares [takes_precision](takes_precision.md) beside it. The library's own answer `true`
for `0`, so that `{}` is always taken.

## Parameters

| Parameter | Description |
|---|---|
| `type` | the type letter of the field, or `0` |

## Return value

`true` when the type takes it.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

struct money { long long cents; };

template<>
struct sgcl::txt::formatter<money> {
    static constexpr bool takes(char type) noexcept {
        return !type || type == 'd';  // d: the whole units only
    }

    static constexpr bool takes_precision() noexcept {
        return false;
    }

    static void write(format_sink& out, money m, const format_spec& spec) {
        char room[32];
        size_t n = spec.type == 'd' ? format_to(room, "{}", m.cents / 100)
                                    : format_to(room, "{}.{:02}", m.cents / 100, m.cents % 100);
        write_padded(out, {room, n}, spec);
    }
};

int main() {
    println("[{:>8}] [{:d}]", money{1250}, money{1250});
    println("{}", txt::fits<money>(txt::runtime("{:x}")));
    return 0;
}
```

Output:

```text
[   12.50] [12]
false
```

## See also

- [takes_precision](takes_precision.md): whether a precision is taken
- [sgcl::txt::formatter](../formatter.md)
