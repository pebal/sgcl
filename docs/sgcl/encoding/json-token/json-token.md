[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [token](../json-token.md)

# sgcl::encoding::json::token::token

```cpp
token() noexcept = default;
```

Constructs a null token with an empty text: a place for a token to be put into later. The tokens of an input are
made by the [reader](../json-reader/next.md), with constructors of its own whose first parameter is a private
type of the token, which a program cannot name. A token is copied and assigned as a slice is, the copy sharing the
text, and none of that throws.

## Parameters

None.

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
    encoding::json::token last;
    println("{} [{}]", last.type() == encoding::json::token::kind::null, last.text());

    encoding::json::reader r(string("[1, 2, 3]"));
    while (auto t = r.next()) {
        if (t->type() == encoding::json::token::kind::number) {
            last = *t;
        }
    }
    println("{}", last.text());
}
```

Output:

```text
true []
3
```

## See also

- [json::reader::next](../json-reader/next.md): what makes the tokens
- [sgcl::encoding::json::token](../json-token.md)
