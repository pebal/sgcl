[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::as

```cpp
template<class T> expected<T, error> as() const;
```

This element as a value of a program's type `T`, described by `describe(field_list&)` and mapped as
[A program's types](../xml.md#a-programs-types) says, or of a type that is text (a number, a string, an enum, a
type with `to_text`/`from_text`), read from the element's text. `T` needs a default constructor.

The error has the path inside the element (`/point/@y`) and no place, as a tree holds no places in a document: no
line, no column, an offset of 0, and a `message()` of the path and the words. `xml()` is `errc::missing_field`.

## Parameters

None.

## Return value

The value; otherwise the [error](../error.md): `errc::type_mismatch`, `errc::out_of_range` or
`errc::missing_field`.

## Complexity

Linear in the size of the subtree.

## Exceptions

What the default constructor of `T` and its `describe` throw.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x = 0;
    int y = 0;

    void describe(encoding::field_list& f) {
        f.add("x", x).attribute();
        f.add("y", y).attribute();
    }
};

int main() {
    auto shape = encoding::xml::parse("<shape><point x='1' y='2'/><point x='3' y='z'/></shape>");
    for (auto p : shape->children("point")) {
        auto v = p.as<point>();
        if (v) {
            println("{} {}", v->x, v->y);
        } else {
            println(v.error().message());
        }
    }
    println(encoding::xml("n", " 42 ").as<int>().value());
    println(shape->child("line").as<point>().error().code() == encoding::errc::missing_field);
}
```

Output:

```text
1 2
/point/@y: expected an integer, found "z"
42
true
```

## See also

- [parse](parse.md): `parse<T>`, a document read into a value
- [from](from.md): the way back
- [field_list](../field_list.md)
- [sgcl::encoding::xml](../xml.md)
