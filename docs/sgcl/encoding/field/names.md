[sgcl](../../README.md) › [encoding](../README.md) › [field](README.md)

# sgcl::encoding::field::names

```cpp
field& names(std::initializer_list<const char*> names) noexcept;
```

Marks an enum field to be written as one of `names` and read from one: the name of the value `v` is `names[v]`,
the values counted from 0. A value past the names is an error when written (`unsupported_value`), a name not
among them when read. Without names an enum is its integer. The names are kept as pointers and read after
`describe` returns: they are literals. The option reaches the elements of a container, the values of a map and
the value of an optional. JSON, XML and CSV write the name alike.

## Parameters

| Parameter | Description |
|---|---|
| `names` | the name of each value of the enum, in the order of the values from 0 |

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

enum class role { reader, writer, admin };

struct member {
    string name;
    vector<role> roles;
    role fallback = role::reader;

    void describe(encoding::field_list& f) {
        f.add("name", name);
        f.add("roles", roles).names({"reader", "writer", "admin"});
        f.add("fallback", fallback);
    }
};

int main() {
    println(encoding::json::stringify(member{"ala", {role::admin, role::writer}}).value());
    auto m = encoding::json::parse<member>(R"({"name": "ola", "roles": ["owner"]})");
    println(m.error().message());
    member wrong{"x", {role(7)}};
    println(encoding::json::stringify(wrong).error().message());
}
```

Output:

```text
{"name":"ala","roles":["admin","writer"],"fallback":0}
1:27 /roles/0: "owner" is none of the field's names
/roles/0: the value 7 has no name
```

## See also

- [tagged](tagged.md): a variant as an object with a tag
- [sgcl::encoding::field](README.md)
