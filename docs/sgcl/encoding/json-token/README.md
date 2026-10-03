[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md)

# sgcl::encoding::json::token

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        class token;
    };
}
```

`sgcl::encoding::json::token` is a token of a [json::reader](../json-reader/README.md), what its
[next](../json-reader/next.md) returns: its [kind](../json-token-kind.md) and its text. The text of a key or of a
string is its characters with the escapes decoded; of a number, its literal as the input wrote it (`1.50e+2`); of
a boolean or null, `true`, `false` or `null`; of a bracket, the bracket. A number is read from its literal only
when it is asked for, as the type the program asks for ([as_int](as_int.md),
[as_uint](as_uint.md), [as_double](as_double.md)), so a literal that no `double` holds
loses nothing on the way.

It is Go's v2 `jsontext.Token`, and what v1's `json.Token` is (a `Delim`, a `bool`, a `float64`, a `Number`, a
`string` or `nil`) with the key told apart from a string.

## Rules

- **The text is a slice of the reader's memory**: the token holds that memory, but the reader writes over it — the
  block of a stream is reused, and a string with escapes is decoded into a scratch block that the next one is
  decoded into. A text kept past the reader's next call is copied, `string(t.text())`.
- A token is copied as a slice is, sharing its text, and lives where a slice may: on a stack or inside a managed
  object.
- The conversions give the value exactly or not at all: `nullopt` for a token of another kind, and for a number
  the type does not hold. Each has a form with a fallback, `t.as_int(0)`.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| v2 `jsontext.Token`, v1 `json.Token` | `json::token`: the kind and the text |
| v2 `Token.Kind()`; v1 a type switch on `Delim`, `bool`, `string`… | `type()`, a [kind](../json-token-kind.md), `key` apart from `string` |
| v2 `Token.String()` | `text()`: a string's characters decoded, a number's literal |
| v2 `Token.Bool()`, `Int()`, `Uint()`, `Float()` | `as_bool()`, `as_int()`, `as_uint()`, `as_double()`: `nullopt` where Go panics, truncates or saturates |

## Member types

| Type | Definition |
|---|---|
| `kind` | [json::token::kind](../json-token-kind.md): what a token is |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](json-token.md) | constructs a null token |

#### Observers

| Function | Description |
|---|---|
| [type](type.md) | the kind of the token |
| [text](text.md) | the text of the token |

#### Conversions

| Function | Description |
|---|---|
| [as_bool](as_bool.md) | the value of a boolean |
| [as_int](as_int.md) | a number as an `int64_t`, when its value is an integer that fits |
| [as_uint](as_uint.md) | a number as an `uint64_t`, when its value is an integer that fits |
| [as_double](as_double.md) | a number as the nearest `double` |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::reader r(string(R"({"id": 18446744073709551615, "ratio": 1.5e1, "on": true})"));
    while (auto t = r.next()) {
        switch (t->type()) {
            case encoding::json::token::kind::key:
                print("{}: ", t->text());
                break;
            case encoding::json::token::kind::number:
                println("{} {} {}", t->as_int().has_value(), t->as_uint(0), t->as_double(0));
                break;
            case encoding::json::token::kind::boolean:
                println("{}", t->as_bool(false));
                break;
            default:
                break;
        }
    }
}
```

Output:

```text
id: false 18446744073709551615 18446744073709551616
ratio: true 15 15
on: true
```

## See also

- [json::reader](../json-reader/README.md): what reads the tokens
- [json::token::kind](../json-token-kind.md): the kinds
- [sgcl::encoding::json](../json/README.md)
