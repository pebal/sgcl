[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::attrs

```cpp
#include "sgcl/slog/record.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class attrs;
}
```

`sgcl::slog::attrs` is the attributes of a group, in order, as a range of [attr](../attr/README.md): what
[value::as_group](../value/as_group.md) returns, slog's `[]Attr` of `Value.Group`. A [record](../record/README.md) walks its
own attributes with the same [iterator](../attrs-iterator.md). The attributes a call wrote stand in the list in the
place a logger's [group](../logger/group.md) left for them, so a walk goes through the logger's and the call's as one.

## Rules

- A view of the record's, valid while the record is: in the record a handler is given, while `handle` runs; in a
  [clone](../record/clone.md), while the clone is. The attributes of a type described by its fields are a copy of their
  own, which the range holds, so they are valid while the range is.
- A range lives where a `tracked_ptr` may: it holds one to what keeps a copy.
- The attributes are given by value, each an `attr`; nothing in the range changes.

## Member types

| Type | Definition |
|---|---|
| [iterator](../attrs-iterator.md) | an input iterator whose `*` is an [attr](../attr/README.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](attrs.md) | constructs an empty range |
| `(destructor)` | drops the range; a copy it holds lives while a range holds it |
| `operator=` | copies the range; it views the same attributes |

#### Iterators

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first attribute |
| [end](end.md) | the iterator past the last attribute |

#### Capacity

| Function | Description |
|---|---|
| [size](size.md) | the number of attributes, walked |
| [empty](empty.md) | checks whether there is no attribute |

## Complexity

`size` walks the attributes; everything else is constant.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", slog::group("req", "id", 7, "path", "/a"));
    slog::attrs fields = (*kept.records()[0].begin()).value().as_group();
    println("{} fields", fields.size());
    for (auto a : fields) {
        println("{} = {}", a.key(), a.value().text());
    }
}
```

Output:

```text
2 fields
id = 7
path = /a
```

## See also

- [attr](../attr/README.md): what the range holds
- [value::as_group](../value/as_group.md), [record](../record/README.md)
- [sgcl::slog](../README.md)
