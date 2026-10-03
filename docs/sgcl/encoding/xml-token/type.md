[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [token](../xml-token.md)

# sgcl::encoding::xml::token::type

```cpp
kind type() const noexcept;
```

What the token is: the start or the end of an element, a text, a comment, an instruction or the DOCTYPE
declaration ([token::kind](../xml-token-kind.md)).

## Parameters

None.

## Return value

The kind of the token.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<p>one <b>two</b> three</p>");
    int texts = 0;
    while (auto t = r.next()) {
        if (t->type() == encoding::xml::token::kind::text) {
            ++texts;
        }
    }
    println(texts);
}
```

Output:

```text
3
```

## See also

- [is_start](is_start.md), [is_end](is_end.md): the start or the end of an element of a name
- [token::kind](../xml-token-kind.md)
- [sgcl::encoding::xml::token](../xml-token.md)
