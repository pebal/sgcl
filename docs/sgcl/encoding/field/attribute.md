[sgcl](../../README.md) › [encoding](../README.md) › [field](README.md)

# sgcl::encoding::field::attribute

```cpp
field& attribute() noexcept;
```

Marks the field as an attribute of the element in XML, not a child element: `<book id="b1">`. An attribute holds
text — a number, a boolean, a string, an enum, a type with `to_text`, or an optional or a pointer of one, left out
when empty — and a field of another kind marked so is `unsupported_value`. JSON and CSV pass over the option.
Go's `xml:",attr"`.

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

struct book {
    string id;
    optional<string> lang;
    string title;

    void describe(encoding::field_list& f) {
        f.add("id", id).attribute();
        f.add("lang", lang).attribute();
        f.add("title", title);
    }
};

int main() {
    println(encoding::xml::stringify("book", book{"b1", nullopt, "Dune"}).value());
    auto b = encoding::xml::parse<book>(R"(<book id="b2" lang="pl"><title>Lalka</title></book>)");
    println("{} {} {}", b->id, *b->lang, b->title);
    println(encoding::json::stringify(*b).value());
}
```

Output:

```text
<book id="b1"><title>Dune</title></book>
b2 pl Lalka
{"id":"b2","lang":"pl","title":"Lalka"}
```

## See also

- [text](text.md): the element's text
- [xml](../xml/README.md): the format that reads it
- [sgcl::encoding::field](README.md)
