[sgcl](../README.md) › [slog](README.md) › [attrs](attrs/README.md)

# sgcl::slog::attrs::iterator

```cpp
#include "sgcl/slog/record.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class attrs {
    public:
        class iterator;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::attrs::iterator` is the input iterator of [attrs](attrs/README.md) and of a [record](record/README.md)
([begin](record/begin.md)): its `*` is an [attr](attr/README.md), made by value when it is asked for. At the place a
logger's group left for the attributes of the call it goes on through those, and after the last it compares equal
to the end.

## Rules

- An iterator views the attributes as the range does: valid while the record is, or while the copy it holds is.
- `*` returns the attribute by value: there is no element in memory to refer to.
- Every iterator at its end is equal to every other one at its end, the default one included.

## Member types

| Type | Definition |
|---|---|
| `iterator_category` | `std::input_iterator_tag` |
| `value_type` | [attr](attr/README.md) |
| `difference_type` | `std::ptrdiff_t` |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | the default iterator, at the end; an iterator of attributes is made by [begin](attrs/begin.md) and [end](attrs/end.md) |
| `operator*` | the attribute it stands at |
| `operator++` | moves to the next attribute |
| `operator==` | checks whether two iterators stand at the same attribute, or both at the end |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::memory kept;
    slog::logger(kept).with("a", 1).info("m", "b", 2);
    auto r = kept.records()[0];
    for (slog::attrs::iterator i = r.begin(); i != r.end(); ++i) {
        println("{} = {}", (*i).key(), (*i).value().text());
    }
}
```

Output:

```text
a = 1
b = 2
```

## See also

- [sgcl::slog::attrs](attrs/README.md)
