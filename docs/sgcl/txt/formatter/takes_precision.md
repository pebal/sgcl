[sgcl](../../README.md) › [txt](../README.md) › [formatter](../formatter.md)

# sgcl::txt::formatter\<T\>::takes_precision

```cpp
static constexpr bool takes_precision() noexcept;
```

Whether the type takes a precision, `.3`: asked where the pattern is read, when the field has one, beside
[takes](takes.md). The library's floating-point numbers and texts take one; its integers, characters, `bool`,
pointers, enumerations, durations, ranges and pairs do not, so `{:.3}` of an integer is an error of the compiler.

## Parameters

None.

## Return value

`true` when the type takes a precision.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

struct ratio { double value; };

template<>
struct sgcl::txt::formatter<ratio> {
    static constexpr bool takes(char type) noexcept {
        return !type;
    }

    static constexpr bool takes_precision() noexcept {
        return true;  // the digits after the point of the percentage
    }

    static void write(format_sink& out, ratio r, const format_spec& spec) noexcept {
        char room[32];
        format_sink number(room, sizeof room - 1);
        format_spec digits{.precision = spec.precision < 0 ? 0 : spec.precision, .type = 'f'};
        formatter<double>::write(number, r.value * 100, digits);
        room[number.size()] = '%';
        format_spec field = spec;
        field.precision = -1;  // spent on the digits, not a cut of the text
        write_padded(out, {room, number.size() + 1}, field);
    }
};

int main() {
    println("[{}] [{:.2}] [{:>8.1}]", ratio{0.25}, ratio{1.0 / 3}, ratio{0.5});
    return 0;
}
```

Output:

```text
[25%] [33.33%] [   50.0%]
```

## See also

- [takes](takes.md): whether a type letter is taken
- [sgcl::txt::formatter](../formatter.md)
