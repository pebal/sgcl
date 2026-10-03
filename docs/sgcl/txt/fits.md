[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::fits\<A...\>

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    template<class... A>
    bool fits(const runtime_pattern& pattern) noexcept;
}
```

Whether a pattern read where the program runs fits the values it will be given, with nothing written: the question
the compiler asks of a literal, asked of a catalogue of translations where it is loaded rather than at every
message. The types are named and the values are not, since there are none yet — `txt::fits<int>(entry)` is what the
program will pass when it comes to it.

`true` exactly when [format](format.md) (2) of values of those types would not answer `nullopt`: every brace is
closed, every field names a value there is, and every value takes its specification.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern, a [runtime](runtime.md) of a text |

## Return value

`true` when the pattern fits values of the types `A...`, `false` when it does not.

## Complexity

Linear in the length of the pattern.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    map<string, string> catalogue = {{"items-left", "pozostało: {}"},
                                     {"broken", "pozostało: {:s}"}};
    for (auto& [key, text] : catalogue) {
        if (!txt::fits<int>(txt::runtime(text))) {
            println("{}: this translation is broken", key);
        }
    }
    return 0;
}
```

Output:

```text
broken: this translation is broken
```

## See also

- [format](format.md): the rules of the pattern
- [runtime](runtime.md), [runtime_pattern](runtime_pattern/README.md): a pattern read where the program runs
