[sgcl](../../README.md) › [encoding](../README.md) › [field_list](../field_list.md)

# sgcl::encoding::field_list::size

```cpp
size_t size() const noexcept;
```

The number of fields [add](add.md) added to the list, a base class's included.

## Parameters

None.

## Return value

The number of fields.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct entity {
    int64_t id = 0;

    void describe(encoding::field_list& f) {
        f.add("id", id);
    }
};

struct person : entity {
    string name;
    string email;

    void describe(encoding::field_list& f) {
        entity::describe(f);
        f.add("name", name);
        f.add("email", email);
    }
};

int main() {
    encoding::field_list fields;
    person p;
    p.describe(fields);
    println("{}", fields.size());
}
```

Output:

```text
3
```

## See also

- [add](add.md): a field added
- [sgcl::encoding::field_list](../field_list.md)
