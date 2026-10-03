[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::stringify

```cpp
template<class T>
static expected<string, error> stringify(const string& name, const T& value,
                                         const style& s = compact);
```

The text of the element `name` made of a value of a program's type: [from](from.md), then
[to_string](to_string.md) with the style `s`. Go's `xml.Marshal(v)`, with the name of the root given here rather
than by an `XMLName` field; `stringify(name, v, xml::pretty)` is `xml.MarshalIndent`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element |
| `value` | the value |
| `s` | the indentation and the XML declaration ([style](../xml-style.md)); `compact` by default |

## Return value

The text; otherwise the [error](../error/README.md) of [from](from.md), `errc::unsupported_value`.

## Complexity

Linear in the size of the value.

## Exceptions

What the `describe` of `T` throws; `length_error` when the text would pass `string::max_size()`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

#include <cmath>

using namespace sgcl;

struct book {
    string id;
    string title;
    vector<string> tags;
    optional<double> price;

    void describe(encoding::field_list& f) {
        f.add("id", id).attribute().required();
        f.add("title", title);
        f.add("tag", tags);
        f.add("price", price);
    }
};

int main() {
    book dune{"7", "Dune", {"sf", "classic"}, 45.5};
    println(encoding::xml::stringify("book", dune).value());
    dune.price = INFINITY;
    dune.tags = {};
    println(encoding::xml::stringify("book", dune, encoding::xml::pretty).value());
}
```

Output:

```text
<book id="7"><title>Dune</title><tag>sf</tag><tag>classic</tag><price>45.5</price></book>
<book id="7">
  <title>Dune</title>
  <price>INF</price>
</book>
```

## See also

- [parse](parse.md): `parse<T>`, the way back
- [save](save.md): the text into a file
- [writer::value](../xml-writer/value.md): a value written onto a stream
- [sgcl::encoding::xml](README.md)
