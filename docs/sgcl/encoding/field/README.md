[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::field

```cpp
#include "sgcl/encoding/fields.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class field;
}
```

`sgcl::encoding::field` is the options of one field of a [field_list](../field_list/README.md), set by chaining them after
[add](../field_list/add.md): `f.add("name", name).required()`, `f.add("age", age).omit_empty()`,
`f.add("access", access).names({"reader", "writer", "admin"})`. Each sets a mark the formats read when they
read or write the field; a field with no options is read and written by the rules of its type
([field_list](../field_list/README.md#rules)). It is what the options of a Go structure's tags are, after the name:
`json:"age,omitempty"`, `json:",string"`, `xml:",attr"`.

## Rules

- **The options are of the field `add` made the `field` for.** A `field` holds the field's place in its list, so
  options are chained right after [add](../field_list/add.md) or set later through a `field` kept with
  `auto f = list.add(...)`, and both reach that field, also when other fields were added since. It is valid while
  `describe` runs, as the list is.
- **An option reaches the values of its field**: `names`, `tagged` and `quoted` apply through an optional, a
  pointer, the elements of a container and the values of a map — a `vector<role>` with names is an array of
  names — but not into the fields of a described type, which have options of their own.
- **Each format takes the options that mean something for it**, and passes over the others:

| Option | JSON | XML | CSV |
|---|---|---|---|
| [required](required.md) | an absent key is `missing_field` | an absent element or attribute is `missing_field` | an absent column is `missing_field` |
| [omit_empty](omit_empty.md) | an empty value is not written | an empty value is not written | not used: every column is written |
| [quoted](quoted.md) | a number or a boolean as a string | not used: every value is text | not used |
| [attribute](attribute.md) | not used | the field is an attribute | not used |
| [text](text.md) | not used | the field is the element's text | not used |
| [names](names.md) | an enum as a name | an enum as a name | an enum as a name |
| [tagged](tagged.md) | a variant as an object with a tag | a variant has no form: `unsupported_value` | a variant has no form: `unsupported_value` |

- A `field` is made by [add](../field_list/add.md) alone, never by the program: it has no public constructor and is
  neither copied nor assigned.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `json:",omitempty"`, v2 `omitzero` | `.omit_empty()` |
| `json:",string"` | `.quoted()` |
| none; v2 has no required | `.required()`: `missing_field` with the path |
| `xml:",attr"`, `xml:",chardata"` | `.attribute()`, `.text()` |
| an enum's `MarshalText` of its names | `.names({...})` |
| an interface in a struct, with a `type` field written by hand | `.tagged("type", {...})` on a `variant` |

## Member functions

#### Options

| Function | Description |
|---|---|
| [required](required.md) | an absent field is an error when read |
| [omit_empty](omit_empty.md) | an empty field is not written |
| [quoted](quoted.md) | a number or a boolean written as a string and read from one |
| [attribute](attribute.md) | XML: the field is an attribute of the element |
| [text](text.md) | XML: the field is the element's text |
| [names](names.md) | an enum written as one of the names |
| [tagged](tagged.md) | a variant written as an object with a tag |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

enum class level { low, high };

struct alert {
    string id;
    int64_t count = 0;
    level severity = level::low;
    string note;

    void describe(encoding::field_list& f) {
        f.add("id", id).required().attribute();
        f.add("count", count).quoted();
        f.add("severity", severity).names({"low", "high"});
        f.add("note", note).omit_empty();
    }
};

int main() {
    alert a{"a1", 12, level::high, ""};
    println(encoding::json::stringify(a).value());
    println(encoding::xml::stringify("alert", a).value());
}
```

Output:

```text
{"id":"a1","count":"12","severity":"high"}
<alert id="a1"><count>12</count><severity>high</severity></alert>
```

## See also

- [field_list](../field_list/README.md): the description of a type
- [json](../json/README.md), [xml](../xml/README.md), [csv::reader](../csv-reader/README.md): the formats that read the options
- [sgcl::encoding](../README.md)
