[sgcl](../../README.md) › [txt](../README.md) › [formatter](README.md)

# sgcl::txt::formatter\<T\>::takes_nested

```cpp
static constexpr bool takes_nested(std::string_view nested) noexcept;
```

Whether the type takes `nested`, the specification after a second colon, for what it holds: `{::>4}` hands `>4` to
the elements. Declaring it is what makes the second colon one: for a type without it, `:` after the first colon is a
character to pad with, as for a number (`{::>6}` of 42 is `::::42`). `nested` is empty but not null for `{::}`, and
a view over nothing when no second colon was written. The type reads it again, its own way, in
[write](write.md), which takes it as a fourth parameter. The library's ranges, pairs, tuples and `optional` ask their
elements in turn, a level for each colon.

## Parameters

| Parameter | Description |
|---|---|
| `nested` | the characters after the second colon, up to the brace |

## Return value

`true` when the type takes it.

## Complexity

Linear in the length of `nested`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

struct row { int cells[3]; };

template<>
struct sgcl::txt::formatter<row> {
    static constexpr bool takes(char type) noexcept {
        return !type;
    }

    static constexpr bool takes_precision() noexcept {
        return false;
    }

    static constexpr bool takes_nested(std::string_view nested) noexcept {
        return nested.empty() || nested == "wide";
    }

    static void write(format_sink& out, const row& r, const format_spec&, std::string_view nested) {
        for (int c : r.cells) {
            char room[16];
            size_t n = nested == "wide" ? format_to(room, "|{:^5}", c) : format_to(room, "|{}", c);
            out.put(room, n);
        }
        out.put('|');
    }
};

int main() {
    println("{} {::wide}", row{{1, 2, 3}}, row{{4, 5, 6}});
    println("{}", txt::fits<row>(txt::runtime("{::narrow}")));
    return 0;
}
```

Output:

```text
|1|2|3| |  4  |  5  |  6  |
false
```

## See also

- [format](../format.md#what-follows-a-second-colon): what follows a second colon
- [sgcl::txt::formatter](README.md)
