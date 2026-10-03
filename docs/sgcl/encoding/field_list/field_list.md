[sgcl](../../README.md) › [encoding](../README.md) › [field_list](README.md)

# sgcl::encoding::field_list::field_list

```cpp
field_list() noexcept;                     // (1)
field_list(const field_list&) = delete;    // (2)
```

Constructs a list. A format makes one for each object it reads or writes and hands it to the object's
`describe`; a program makes one only to look at what a `describe` adds.

1. An empty list. Its first sixteen fields are kept in the list itself, the ones past them in a block of their
   own: a type of up to sixteen fields is described with nothing allocated, once for every record of an array or
   every row of a CSV file.
2. A list is neither copied nor moved: the [field](../field/README.md) that [add](add.md) returns refers to it.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x = 0;
    int y = 0;

    void describe(encoding::field_list& f) {
        f.add("x", x);
        f.add("y", y);
    }
};

int main() {
    encoding::field_list fields;
    println("{}", fields.size());
    point p;
    p.describe(fields);
    println("{}", fields.size());
}
```

Output:

```text
0
2
```

## See also

- [add](add.md): a field added
- [sgcl::encoding::field_list](README.md)
