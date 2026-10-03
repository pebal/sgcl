[sgcl](../README.md) › [core](README.md)

# sgcl::parse\<T\>

```cpp
#include "sgcl/core/string.h"   // or "sgcl/core.h"

namespace sgcl {
    /*(1)*/ template<class T>
            requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
            expected<T, number_error> parse(std::string_view text, int base = 10) noexcept;
    /*(2)*/ template<class T>
            requires std::is_floating_point_v<T>
            expected<T, number_error> parse(std::string_view text) noexcept;
    /*(3)*/ template<class T>
            requires std::is_same_v<T, bool>
            expected<T, number_error> parse(std::string_view text) noexcept;
}
```

Reads a number of type `T` from its text, the reverse of [to_string](to_string.md): `parse<int>("42")`,
`parse<double>("2.5")`, `parse<bool>("true")`. The text must be exactly one number of the type: no white space, no
`+`, no sign for an unsigned type, nothing after the digits, and a value the type holds. Otherwise the result is a
[number_error](number_error.md) that says why and the byte where the reading stopped. What C#'s `TryParse`, Go's
`strconv` and Java's `parseInt` do, without the exception: `std::from_chars` reads the number, so no locale applies
and nothing is allocated.

1. An integer in base `base`, from 2 to 36: `parse<int>("ff", 16)` is 255. A `-` is taken for a signed type; a base
   outside 2 to 36 reads no number.
2. A floating-point number as `std::from_chars` reads it in its general form: a decimal with an optional exponent,
   `inf`, `nan`. A hexadecimal form is not read.
3. `true` from `"true"` and `false` from `"false"`, nothing else.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text of the number; a [string](string.md), a slice of one or a literal converts to it |
| `base` | the base of the integer, from 2 to 36 |

## Return value

The number, or the [number_error](number_error.md) with its reason and its offset in `text`:

- `empty` at 0 when `text` is empty;
- `not_a_number` at 0 when `text` does not begin as a number of the type (and for every text in a base outside 2 to
  36);
- `trailing` at the first byte after the number when more follows it;
- `out_of_range` at the first byte after the number when the type cannot hold it (a floating-point number too small
  as well as too large).

## Complexity

Linear in the length of `text`.

## Exceptions

None.

## Notes

A string converts to `std::string_view`, so `parse<int>(s)` reads a string as it is, and a field of a
[split](string/split.md) the same way, with no copy.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string fields = "42,ff,2.5,true,12px, 7";
    vector<string_slice> parts(fields.split(','));
    println("{} {} {} {}", *parse<int>(parts[0]), *parse<int>(parts[1], 16), *parse<double>(parts[2]),
            *parse<bool>(parts[3]));

    for (string_slice part : {parts[4], parts[5]}) {
        auto n = parse<int>(part);
        println("{}: {} at {}", part, n.error().message(), n.error().offset());
    }
    println("{}", parse<uint8_t>("300").error().message());
}
```

Output:

```text
42 255 2.5 true
12px: more after the number at 2
 7: not a number at 0
a number out of the type's range
```

## See also

- [number_error](number_error.md): why a text is not a number
- [to_string](to_string.md): a number as a string
- [expected](expected.md): the result
- [sgcl::string](string.md)
