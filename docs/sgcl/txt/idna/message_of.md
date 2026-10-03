[sgcl](../../README.md) › [txt](../README.md) › [idna](README.md)

# sgcl::txt::idna::message_of

```cpp
constexpr const char* message_of(error e) noexcept;
```

Returns the rule of an [error](../idna-error.md) in words, a line for a log or a message:
`"a label longer than 63 bytes"`. Not a sentence: the caller writes the sentence, this says which rule it was.
[failure::message](../idna-failure/message.md) gives the same as a string.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the error |

## Return value

The rule in words, a static text; `"no error"` for `error::none`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    using txt::idna::error;
    for (auto e : {error::none, error::hyphen, error::bidi, error::label_too_long}) {
        println("{}", txt::idna::message_of(e));
    }
}
```

Output:

```text
no error
a hyphen in the third and fourth place, or at an end
a label that breaks the bidirectional rule of RFC 5893
a label longer than 63 bytes
```

## See also

- [error](../idna-error.md): the rules
- [sgcl::txt::idna](README.md)
