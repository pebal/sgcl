[sgcl](../../README.md) › [core](../README.md)

# sgcl::number_error

```cpp
#include "sgcl/core/string.h"   // or "sgcl/core.h"

namespace sgcl {
    class number_error;
}
```

`sgcl::number_error` is why [parse](../parse.md) read no number from a text: the [reason](../number_error-reason.md) and
the offset of the byte where the reading stopped, in the `expected` that `parse` returns. It is a value of two fields
and no allocation, compared by both. Its [code](code.md) is the `std::errc` that `std::from_chars`
reports for the same text, for code that speaks in error codes, and its [message](message.md) a short
text in English for a person.

## Rules

- A plain value: trivially copyable, it lives anywhere.

## Member types

| Type | Definition |
|---|---|
| [reason](../number_error-reason.md) | why the text is not a number of the type, `enum class reason : uint8_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](number_error.md) | constructs an error of a reason at an offset |

#### Observers

| Function | Description |
|---|---|
| [why](why.md) | the reason |
| [offset](offset.md) | the byte of the text where the reading stopped |
| [code](code.md) | the reason as a `std::errc` |
| [message](message.md) | the reason as a text |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares the reasons and the offsets |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (string text : {"", "abc", "12abc", "99999999999"}) {
        auto n = parse<int>(text);
        number_error e = n.error();
        println("\"{}\": {} at {}", text, e.message(), e.offset());
    }
}
```

Output:

```text
"": an empty text at 0
"abc": not a number at 0
"12abc": more after the number at 2
"99999999999": a number out of the type's range at 11
```

## See also

- [parse](../parse.md): a number from its text
- [expected](../expected/README.md): the result that holds the number or the error
