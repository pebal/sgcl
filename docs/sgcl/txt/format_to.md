[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_to

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    /*(1)*/ template<class... A>
            size_t format_to(const slice<char>& buffer,
                             const format_pattern<std::type_identity_t<A>...>& pattern,
                             const A&... args) noexcept(/* see below */);
    /*(2)*/ template<class... A>
            optional<size_t> format_to(const slice<char>& buffer, const runtime_pattern& pattern,
                                       const A&... args) noexcept(/* see below */);
}
```

The text of [format](format.md), written into memory the caller lends, for a text that lives no longer than the call
that reads it. What fits is written and **what the whole text takes** comes back, whether or not it fitted — so a
buffer can be sized from a first call with an empty one, and a caller who knows its text is short pays no
allocation at all.

1. A pattern written in the program, read where the program is compiled, as by [format](format.md) (1).
2. A pattern read where the program runs, [runtime](runtime.md): `nullopt` when it does not fit the values, as by
   [format](format.md) (2); what was written into `buffer` by then is left there and means nothing.

A value may not lie in `buffer`: the text is written over it as it is made.

Both are `noexcept` when the writing of every value cannot throw: a `bool`, a character, a floating-point number,
text, a [duration](../core/duration.md), a pointer and an enumeration, a range, a pair or an `optional` of them, and
a type of the program whose `format_value` or [formatter](formatter.md) says `noexcept`. An integer is not among
them, nor anything holding one: its `{:c}` throws `out_of_range` for a number no `char` holds.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | the memory to write into; may be empty |
| `pattern` | the pattern: a literal (1), or a [runtime](runtime.md) of a text (2) |
| `args` | the values of the fields |

## Return value

1. The size of the whole text in bytes; when it is larger than `buffer`, only the first `buffer.size()` bytes were
   written.
2. The same, or `nullopt` when the pattern does not fit the values.

## Complexity

Linear in the length of the text, as [format](format.md); nothing is allocated, but for a field with a width over a
value made of other values, whose body goes first into room the thread keeps.

## Exceptions

- `out_of_range` when a `{:c}` field is given an integer no `char` holds.
- What a `format_value` or a [formatter](formatter.md) of the program throws.

None when the values' writing is `noexcept`.

## Example

Into memory the caller lends, with what the whole text takes:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[64];
    int n = 3;
    size_t needed = txt::format_to(room, "{} left", n);
    if (needed > sizeof room) {
        println("the text was cut; ask for {} bytes", needed);
        return 1;
    }
    println("{} ({} bytes)", std::string_view(room, needed), needed);
    return 0;
}
```

Output:

```text
3 left (6 bytes)
```

A buffer too small, and a pattern read where the program runs:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[8];
    size_t needed = txt::format_to(room, "{}, {}", "Lovelace", "Ada");
    println("{} of {} bytes: {}", sizeof room, needed, std::string_view(room, sizeof room));

    optional<size_t> swapped = txt::format_to(room, txt::runtime("{1} {0}"), 1, 2);
    println("{}", std::string_view(room, *swapped));
    println("{}", txt::format_to(room, txt::runtime("{2}"), 1, 2).has_value());
    return 0;
}
```

Output:

```text
8 of 13 bytes: Lovelace
2 1
false
```

## See also

- [format](format.md): the same into a string, and the rules of the pattern
- [runtime](runtime.md): a pattern read where the program runs
- [growing_sink](growing_sink.md): room that grows, for a long text written in steps
- [render_to](stencil/render_to.md): a page of a template into memory the caller lends
