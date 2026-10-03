[sgcl](../../README.md) › [txt](../README.md) › [formatter](README.md)

# sgcl::txt::formatter\<T\>::write

```cpp
static void write(format_sink& out, const T& value, const format_spec& spec);    // (1)
static void write(format_sink& out, const T& value, const format_spec& spec,     // (2)
                  std::string_view nested);
```

Writes `value` into `out` as `spec` says. The specification has been checked by [takes](takes.md) and
[takes_precision](takes_precision.md) by then, so `write` meets only what the type said it takes. The fill, the
alignment and the width are the type's to honour, which [write_padded](../write_padded.md) does for a text. A value
may be taken by value as well as by reference, and the function may be `noexcept`, which keeps
[format_to](../format_to.md) `noexcept` for the type; the library's are, but for the integers', whose `{:c}` throws
`out_of_range` for a number no `char` holds.

1. The form of a type that holds nothing.
2. The form of a type with [takes_nested](takes_nested.md) or [takes_layout](takes_layout.md): `nested` is the
   specification after the second colon, or the pattern from the first `%`, or a view over nothing. Where both forms
   are declared, this one is called.

A text longer than 256 bytes is written twice by [format](../format.md), so `write` is called twice for it: it
writes the same characters both times.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the sink to write into |
| `value` | the value |
| `spec` | the specification of the field |
| `nested` | what follows the second colon, or the pattern |

## Return value

None.

## Complexity

The type's own.

## Exceptions

The type's own; none when it is `noexcept`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

struct initials { char first, last; };

template<>
struct sgcl::txt::formatter<initials> {
    static constexpr bool takes(char type) noexcept {
        return !type || type == 's';
    }

    static constexpr bool takes_precision() noexcept {
        return false;
    }

    static void write(format_sink& out, initials v, const format_spec& spec) noexcept {
        char room[4] = {v.first, '.', v.last, '.'};
        write_padded(out, {room, 4}, spec);
    }
};

int main() {
    char room[32];
    initials ada{'A', 'L'};
    constexpr txt::format_pattern<initials> plain("{}");
    println("[{:>8}] {}", ada, noexcept(txt::format_to(room, plain, ada)));
    return 0;
}
```

Output:

```text
[    A.L.] true
```

## See also

- [write_padded](../write_padded.md): a text in its field
- [sgcl::txt::formatter](README.md)
