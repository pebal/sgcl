[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::from

```cpp
template<class T> static expected<xml, error> from(const string& name, const T& value);
```

The element `name` made of a value of a program's type, described by `describe(field_list&)` and mapped as
[A program's types](README.md#a-programs-types) says: its fields as child elements, attributes and text, in the
order `describe` names them. A value that is text (a number, a string, an enum) is `<name>text</name>`. The tree
may be changed further, put into another, or written by [to_string](to_string.md).

What has no form in XML is `errc::unsupported_value` with the path of the value: a map, a tuple, a variant, a list
of lists, a `json` value; a list where the element is asked for (the root, an optional's value), which has no
element of its own: a list is a field's element repeated; a name that is not the name of an element; an
empty optional or a null pointer, which has no element; nesting deeper than 512 elements. The error has no place
in a text: its `message()` is the path and the words.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element |
| `value` | the value |

## Return value

The element; otherwise the [error](../error/README.md), `errc::unsupported_value`.

## Complexity

Linear in the size of the value.

## Exceptions

What the `describe` of `T` throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct track {
    string title;
    int seconds = 0;

    void describe(encoding::field_list& f) {
        f.add("title", title);
        f.add("seconds", seconds).attribute();
    }
};

struct tagged {
    map<string, string> tags;

    void describe(encoding::field_list& f) {
        f.add("tags", tags);
    }
};

int main() {
    encoding::xml t = encoding::xml::from("track", track{"Intro", 95}).value();
    encoding::xml album = encoding::xml("album").push_back(t).push_back(t.set("seconds", "96"));
    println(album.to_string());
    println(encoding::xml::from("n", 7)->to_string());
    println(encoding::xml::from("t", tagged{}).error().message());
}
```

Output:

```text
<album><track seconds="95"><title>Intro</title></track><track seconds="96"><title>Intro</title></track></album>
<n>7</n>
/t/tags: an object of this kind (a map, a tuple, a variant, json) has no form in XML
```

## See also

- [stringify](stringify.md): the text of the element at once
- [as](as.md): the way back
- [writer::value](../xml-writer/value.md): a value written onto a stream
- [sgcl::encoding::xml](README.md)
