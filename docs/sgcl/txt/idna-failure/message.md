[sgcl](../../README.md) › [txt](../README.md) › [idna](../idna.md) › [failure](../idna-failure.md)

# sgcl::txt::idna::failure::message

```cpp
string message() const noexcept;
```

Returns the rule the name broke in words, as a string: [message_of](../idna/message_of.md)`(rule)`. A line for a log
or a message, not a sentence.

## Parameters

None.

## Return value

The rule in words; `"no error"` for `error::none`.

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
    auto host = txt::idna::to_ascii(string("a").repeat(64) + ".com");
    println("{}", host.error().message());
    println("{}", txt::idna::failure{}.message());
}
```

Output:

```text
a label longer than 63 bytes
no error
```

## See also

- [message_of](../idna/message_of.md): the same of an error
- [sgcl::txt::idna::failure](../idna-failure.md)
