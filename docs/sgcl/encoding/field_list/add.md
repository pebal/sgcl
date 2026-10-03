[sgcl](../../README.md) › [encoding](../README.md) › [field_list](../field_list.md)

# sgcl::encoding::field_list::add

```cpp
template<class Name, class T>
requires std::same_as<Name, const char*> || std::same_as<Name, char*>
field add(Name name, T& value) noexcept;                                            // (1)
template<size_t N, class T> field add(const char (&name)[N], T& value) noexcept;    // (2)
```

Adds a field of the object: its name in the formats and the member itself, in the order the fields are written.
The list keeps the name as a pointer and the member as its address, and the format reads both after `describe`
returns: the name is a literal, or characters that live as long; the member is a member of the object `describe`
was called on. The member's type is checked at compile time against the [types a field may
have](../field_list.md#the-types-a-field-may-have).

1. A name given as a pointer, measured to its first NUL.
2. A literal or a character array: its length is its type's, so no name is measured for every value described.
   An array that holds a shorter string (a zero before its last character) is measured as a pointer is.

The [field](../field.md) returned takes the options of the field, chained: `f.add("age", age).omit_empty()`. It
refers to the field by its place in the list, so a `field` kept (`auto opts = f.add("age", age)`) sets the options
of that field even after other fields were added.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name: the key in JSON, the element or the attribute in XML, the column of CSV's header |
| `value` | the member |

## Return value

The options of the field just added, to be chained or kept while `describe` runs.

## Complexity

Amortized constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

class account {
public:
    account() = default;
    account(const string& owner, int64_t cents) : _owner(owner), _cents(cents) {}

    void describe(encoding::field_list& f) {
        f.add("owner", _owner);
        f.add("balance", _cents);
    }

private:
    string _owner;
    int64_t _cents = 0;
};

int main() {
    println(encoding::json::stringify(account("ala", 1250)).value());
    auto back = encoding::json::parse<account>(R"({"balance": 99, "owner": "ola"})");
    println(encoding::json::stringify(*back).value());
}
```

Output:

```text
{"owner":"ala","balance":1250}
{"owner":"ola","balance":99}
```

## See also

- [field](../field.md): the options of a field
- [size](size.md): the number of fields
- [sgcl::encoding::field_list](../field_list.md)
