[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [token](README.md)

# sgcl::encoding::json::token::text

```cpp
const slice<const char>& text() const noexcept;
```

The text of the token: of a key or a string, its characters with the escapes decoded (`"a\nb"` is three
characters); of a number, its literal as the input wrote it (`1.50e+2`, `-0`); of a boolean or null, `true`,
`false` or `null`; of a bracket, the bracket. Go's v2 `Token.String()`.

The text is a slice of the reader's memory. It holds that memory, but the reader writes over it: the block of a
stream is reused for the blocks after it, and a string with escapes is decoded into a scratch block that the next
one is decoded into. A text kept past the reader's next call is copied, `string(t.text())`.

## Parameters

None.

## Return value

The text of the token; empty for a token made by the default constructor.

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
    encoding::json::reader r(string(R"(["one\"1", "two\"2", 1.50e+2])"));
    r.next();
    auto first = r.next();
    string kept(first->text());
    auto second = r.next();
    auto third = r.next();
    println("{} {} {}", kept, second->text(), third->text());
    println("{}", first->text());  // its scratch block reused by the second string
}
```

Output:

```text
one"1 two"2 1.50e+2
two"2
```

## See also

- [type](type.md): the kind of the token
- [as_int](as_int.md), [as_double](as_double.md): a number's value
- [slice](../../core/slice/README.md)
- [sgcl::encoding::json::token](README.md)
