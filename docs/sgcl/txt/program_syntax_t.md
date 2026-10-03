[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::program_syntax_t

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct program_syntax_t {};

    inline constexpr program_syntax_t program_syntax {};
}
```

The tag of the profile of [UAX #31](https://www.unicode.org/reports/tr31/) §2.3 that every parser of a programming
language wants, passed to [is_identifier](is_identifier.md), [is_identifier_start](is_identifier_start.md) and
[is_identifier_continue](is_identifier_continue.md): the underscore begins a name and the dollar sign stands anywhere
in one, which is what C, C++, Java, JavaScript and the shells all do. `XID_Start` has neither; `XID_Continue` has the
underscore and not the dollar sign.

## Rules

- A **tag, not a flag**: the walk has no branch at run time, and the call reads as an enumerator would,
  `is_identifier(name, txt::program_syntax)`.
- An empty type, passed by value, written by its constant.

## Non-member functions

#### Constants

| Constant | Value | Description |
|---|---|---|
| `program_syntax` | `program_syntax_t{}` | the profile with `_` and `$`, `inline constexpr program_syntax_t` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include <type_traits>

using namespace sgcl;

int main() {
    for (auto s : {"_name", "$el", "wartość"}) {
        println("{}: {} {}", s, txt::is_identifier(s), txt::is_identifier(s, txt::program_syntax));
    }
    println("{}", std::is_empty_v<txt::program_syntax_t>);
}
```

Output:

```text
_name: false true
$el: false true
wartość: true true
true
```

## See also

- [is_identifier](is_identifier.md): whether a text is an identifier
- [txt](README.md)
