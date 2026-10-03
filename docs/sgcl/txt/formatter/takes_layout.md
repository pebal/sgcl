[sgcl](../../README.md) › [txt](../README.md) › [formatter](README.md)

# sgcl::txt::formatter\<T\>::takes_layout

```cpp
static constexpr bool takes_layout(std::string_view pattern) noexcept;
```

Whether the type takes `pattern`, a pattern of its own after the specification: declaring it makes the field read
as `std::format` reads one of `<chrono>`, `[[fill]align][width]` and then everything from the first `%` to the
brace, colons included, so `{:%H:%M}` is one field and not a nest. `pattern` is a view over nothing when none was
written. The pattern is checked here where the program is compiled, like any other specification, and handed to
[write](write.md) as its fourth parameter; a sign, `#`, `0` and a type are not read for such a type. It is what the
formatters of [time](../../time/README.md#formatting-with-txt) declare, and a list or an `optional` of the type hands
the pattern on.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the characters from the first `%` to the brace, or a view over nothing |

## Return value

`true` when the type takes it.

## Complexity

Linear in the length of `pattern`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

struct temperature { double celsius; };

template<>
struct sgcl::txt::formatter<temperature> {
    static constexpr bool takes(char type) noexcept {
        return !type;
    }

    static constexpr bool takes_precision() noexcept {
        return false;
    }

    static constexpr bool takes_layout(std::string_view pattern) noexcept {
        return !pattern.data() || pattern == "%C" || pattern == "%F";
    }

    static void write(format_sink& out, temperature t, const format_spec& spec,
                      std::string_view pattern) noexcept {
        bool f = pattern == "%F";
        double degrees = f ? t.celsius * 9 / 5 + 32 : t.celsius;
        char room[32];
        size_t n = format_to(room, "{:.1f}°{}", degrees, f ? 'F' : 'C');
        write_padded(out, {room, n}, spec);
    }
};

int main() {
    println("{} {:%F} [{:>8%C}]", temperature{21.5}, temperature{21.5}, temperature{-3});
    println("{}", txt::fits<temperature>(txt::runtime("{:%K}")));
    return 0;
}
```

Output:

```text
21.5°C 70.7°F [  -3.0°C]
false
```

## See also

- [format](../format.md#a-pattern-of-time): a pattern of time
- [sgcl::txt::formatter](README.md)
