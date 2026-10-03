[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::write_padded

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    constexpr void write_padded(format_sink& out, std::string_view text,
                                const format_spec& spec) noexcept;
}
```

Writes `text` in its field, as a string with the same specification is written: the fill, the alignment (to the left
unless the specification says otherwise), the width measured in columns rather than bytes, and a precision that cuts
the text to that many columns on a grapheme cluster. It is what the `format_value` of a type of one's own calls once
it has its text, so that the field of `{:>10}` or `{:*^12}` is honoured without a line of padding written by the
type. The sign, `#`, `0` and the type of the specification are not read.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the sink the value is written into |
| `text` | the text: a string, a slice, a literal, or the characters a type wrote into a buffer of its own, `{room, n}` |
| `spec` | the specification of the field |

## Return value

None.

## Complexity

Linear in the length of `text` and in the width.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

namespace weather {
    enum class sky { clear, cloudy, overcast };

    void format_value(txt::format_sink& out, sky s, const txt::format_spec& spec) {
        static const char* names[] = {"słonecznie", "pochmurno", "zachmurzenie"};
        txt::write_padded(out, names[int(s)], spec);
    }
}

int main() {
    println("[{:<12}] [{:*^12}] [{:.4}]", weather::sky::clear, weather::sky::cloudy,
            weather::sky::overcast);
    return 0;
}
```

Output:

```text
[słonecznie  ] [*pochmurno**] [zach]
```

## See also

- [format](format.md#a-type-of-ones-own): a type of one's own
- [format_sink](format_sink/README.md), [format_spec](format_spec.md): what it is handed
