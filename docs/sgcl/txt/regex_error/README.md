[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::regex_error

```cpp
#include "sgcl/txt/regex.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class regex_error;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::regex_error` is why a pattern given to [regex::compile](../regex/compile.md) is not one: the sentence and
the byte of the pattern where it went wrong. The sentence is the point — "a lookahead or a lookbehind: this engine
matches in time linear in the length of the text" tells a reader what to do, and an empty `optional` does not. It
is the error of the `expected` that `compile` returns; it is a value, not an exception, and nothing throws it.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](regex_error.md) | constructs the error from a sentence and an offset |
| `(destructor)` | drops the sentence |

#### Observers

| Function | Description |
|---|---|
| [message](message.md) | the sentence, with the place in the pattern |
| [offset](offset.md) | the byte of the pattern where it went wrong |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string typed = "(?i)kot(?=ek)";
    auto re = txt::regex::compile(typed);
    if (!re) {
        println("{}", typed);
        println("{}^", string(" ").repeat(re.error().offset()));
    }
}
```

Output:

```text
(?i)kot(?=ek)
       ^
```

## See also

- [regex::compile](../regex/compile.md): what gives the error, and the syntax
- [regex](../regex/README.md)
