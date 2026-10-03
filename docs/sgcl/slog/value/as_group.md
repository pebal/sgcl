[sgcl](../../README.md) › [slog](../README.md) › [value](README.md)

# sgcl::slog::value::as_group

```cpp
attrs as_group() const;
```

Returns the attributes of a value of kind `group`, in order, as a range of [attr](../attr/README.md), [attrs](../attrs/README.md): a [group](../group/README.md)'s pairs, what a logger's group holds, or the fields of a type described by them, copied first into memory of the range's own, which it keeps. A value of another kind is a mistake of the program: a `logic_error`, where Go panics.

## Parameters

None.

## Return value

The range of the attributes, an [attrs](../attrs/README.md).

## Complexity

Constant for a group; linear in the size of a described type, which is copied.

## Exceptions

`logic_error` when [type](type.md) is not `group`: `sgcl::slog::value::as_group: a value of another kind`. What a field of a described type read through its operations throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct request {
    int id = 0;
    string path;

    void describe(encoding::field_list& f) {
        f.add("id", id);
        f.add("path", path);
    }
};

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "req", request{5, "/a"});
    auto fields = (*kept.records()[0].begin()).value().as_group();
    println("{} fields", fields.size());
    for (auto a : fields) {
        println("{} = {}", a.key(), a.value().text());
    }
}
```

Output:

```text
2 fields
id = 5
path = /a
```

## See also

- [type](type.md)
- [attrs](../attrs/README.md): the range
- [kind](../value-kind.md): `group`
- [sgcl::slog::value](README.md)
