[sgcl](../../README.md) › [encoding](../README.md) › [field](../field.md)

# sgcl::encoding::field::tagged

```cpp
field& tagged(const char* key, std::initializer_list<const char*> names) noexcept;
```

Marks a `variant` field to be written as an object with a tag: the member `key` with the name of the alternative
held, then the fields of the alternative, each alternative a described type — `{"type": "circle", "r": 1}`.
Reading finds the tag wherever it is among the members and reads the rest into that alternative; a tag that is
none of `names` is an error, and so is an object without the tag (`missing_field`). A variant without `tagged` is
`unsupported_value`, read and written. The alternatives need no `==`. The key and the names are kept as pointers and read after `describe`
returns: they are literals. The option reaches the elements of a container. JSON alone has it: XML and CSV have no
form for a variant. What Go writes by hand for an interface in a struct: a `type` field and a switch on it.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the name of the member that holds the tag |
| `names` | the tag of each alternative, in the order of the variant's alternatives |

## Return value

`*this`, for the next option in the chain.

## Complexity

Linear in the number of names.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct circle {
    double r = 0;

    void describe(encoding::field_list& f) {
        f.add("r", r);
    }
};

struct square {
    double side = 0;

    void describe(encoding::field_list& f) {
        f.add("side", side);
    }
};

struct drawing {
    vector<variant<circle, square>> shapes;

    void describe(encoding::field_list& f) {
        f.add("shapes", shapes).tagged("type", {"circle", "square"});
    }
};

int main() {
    drawing d{{circle{1}, square{2}}};
    println(encoding::json::stringify(d).value());

    auto back = encoding::json::parse<drawing>(R"({"shapes": [{"side": 3, "type": "square"}]})");
    println("{} {}", back->shapes[0].index(), get<square>(back->shapes[0]).side);
    auto oval = encoding::json::parse<drawing>(R"({"shapes": [{"type": "oval"}]})");
    println(oval.error().message());
}
```

Output:

```text
{"shapes":[{"type":"circle","r":1},{"type":"square","side":2}]}
1 3
1:22 /shapes/0/type: "oval" is none of the alternatives
```

## See also

- [names](names.md): an enum as a name
- [sgcl::encoding::field](../field.md)
