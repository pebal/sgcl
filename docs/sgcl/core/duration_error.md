[sgcl](../README.md) › [core](README.md)

# sgcl::duration_error

```cpp
#include "sgcl/core/duration.h"   // or "sgcl/core.h"

namespace sgcl {
    class duration_error;
}
```

`sgcl::duration_error` is why a text is not a [duration](duration.md): a sentence and the byte of the text the
reading stopped on. It is the error of [duration::parse](duration/parse.md), and what the
`bad_expected_access<duration_error>` thrown by the constructor of a duration from a wrong literal carries. The
sentence is a literal, so the error is two words and costs nothing to make or to copy.

## Rules

- A plain value of two words, trivially copyable: it lives anywhere.
- The sentences are the library's: `"a number expected"`, `"a unit expected: ns, us, ms, s, m or h"`,
  `"an unknown unit: ns, us, ms, s, m or h expected"`, `"out of range: a duration is at most about 292 years"`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](duration_error/duration_error.md) | constructs an error of a sentence and an offset |
| [message](duration_error/message.md) | the sentence |
| [offset](duration_error/offset.md) | the byte of the text the reading stopped on |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string setting = "15 minutes";
    auto timeout = duration::parse(setting);
    if (!timeout) {
        const duration_error& e = timeout.error();
        println("\"{}\": {} at byte {}", setting, e.message(), e.offset());
    }
}
```

Output:

```text
"15 minutes": an unknown unit: ns, us, ms, s, m or h expected at byte 2
```

## See also

- [duration::parse](duration/parse.md): what returns it
- [expected](expected.md): the value or the error
- [sgcl::duration](duration.md)
