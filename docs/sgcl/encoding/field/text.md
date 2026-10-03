[sgcl](../../README.md) › [encoding](../README.md) › [field](../field.md)

# sgcl::encoding::field::text

```cpp
field& text() noexcept;
```

Marks the field as the text of the element in XML, not a child element: `<price currency="EUR">12.5</price>`. The
text holds what an attribute holds — a number, a boolean, a string, an enum, a type with `to_text`, or an optional
or a pointer of one — and a field of another kind marked so is `unsupported_value`; an empty text is not written.
JSON and CSV pass over the option. Go's `xml:",chardata"`.

## Parameters

None.

## Return value

`*this`, for the next option in the chain.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct price {
    string currency;
    double amount = 0;

    void describe(encoding::field_list& f) {
        f.add("currency", currency).attribute();
        f.add("amount", amount).text();
    }
};

int main() {
    println(encoding::xml::stringify("price", price{"EUR", 12.5}).value());
    auto p = encoding::xml::parse<price>(R"(<price currency="PLN">99.9</price>)");
    println("{} {}", p->amount, p->currency);
}
```

Output:

```text
<price currency="EUR">12.5</price>
99.9 PLN
```

## See also

- [attribute](attribute.md): an attribute of the element
- [xml](../xml.md): the format that reads it
- [sgcl::encoding::field](../field.md)
