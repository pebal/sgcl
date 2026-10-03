[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [token](../json-token.md)

# sgcl::encoding::json::token::type

```cpp
kind type() const noexcept;
```

The [kind](../json-token-kind.md) of the token: a bracket, a key, a string, a number, a boolean or null. Go's v2
`Token.Kind()`.

## Parameters

None.

## Return value

The kind of the token; `kind::null` for a token made by the default constructor.

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
    encoding::json::reader r(string(R"({"name": "ala", "pets": ["cat"]})"));
    int keys = 0;
    int strings = 0;
    while (auto t = r.next()) {
        if (t->type() == encoding::json::token::kind::key) {
            ++keys;
        } else if (t->type() == encoding::json::token::kind::string) {
            ++strings;
        }
    }
    println("{} keys, {} strings", keys, strings);
}
```

Output:

```text
2 keys, 2 strings
```

## See also

- [json::token::kind](../json-token-kind.md): the kinds
- [text](text.md): the text of the token
- [sgcl::encoding::json::token](../json-token.md)
