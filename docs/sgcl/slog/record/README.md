[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::record

```cpp
#include "sgcl/slog/record.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class record;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::slog::record` is a record as a [handler](../handler/README.md) of the program gets it, slog's `Record`: the time, the
level, the message, where it was written, and the attributes as a range of [attr](../attr/README.md) — the logger's (from
[with](../logger/with.md), inside its groups) and the call's, as a tree in which a logger's
[group](../logger/group.md) is an attribute of kind group that holds what came after it. Go's handler gets the
logger's attributes apart, through `WithAttrs` and `WithGroup`; here the record says it all.

## Rules

- **A view.** The record, its attributes and their values point at the stack of the call and at the objects it
  named: valid while `handle` runs, and not to be kept.
- **A copy to keep.** [clone](clone.md) is a copy that owns all it holds — its texts, its attributes, a
  described type made a group of copies, a container of the program its two texts. [memory](../memory/README.md) keeps those.
- **The tree.** The attributes at the top are `with`'s before a `group`, then the group, an attribute of kind group
  named after it holding what came after it; the call's own attributes are inside the innermost group.
## Member functions

| Function | Description |
|---|---|
| [(constructor)](record.md) | constructs an empty record |
| `(destructor)` | drops the record; a clone's copy lives while a record holds it |
| `operator=` | copies the record; a view stays a view of the same call |

#### Observers

| Function | Description |
|---|---|
| [time](time.md) | the time, in the logger's zone |
| [level](level.md) | the level |
| [message](message.md) | the text of the message |
| [source](source.md) | where the call is |
| [has_source](has_source.md) | checks whether the logger writes where the call is |

#### Attributes

| Function | Description |
|---|---|
| [begin](begin.md) | an iterator to the first attribute at the top of the tree |
| [end](end.md) | the iterator past the last attribute |
| [size](size.md) | the number of attributes at the top of the tree |
| [empty](empty.md) | checks whether the record has no attributes |

#### Copies

| Function | Description |
|---|---|
| [clone](clone.md) | a copy that owns all it holds |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct tree {
    void handle(const slog::record& r) const {
        println("{} {}:", int(r.level()), r.message());
        for (auto a : r) {
            println("  {} = {}", a.key(), a.value().text());
        }
    }
};

int main() {
    auto log = slog::logger(tree()).with("service", "api").group("req").with("id", 7);
    log.warn("slow", "took", 250 * millisecond);
}
```

Output:

```text
4 slow:
  service = api
  req = [id=7 took=250ms]
```

## See also

- [handler](../handler/README.md), [memory](../memory/README.md)
- [attr](../attr/README.md), [value](../value/README.md): what the range holds
- [attrs](../attrs/README.md): the attributes of a group, and the iterator a record walks with
- [sgcl::slog](../README.md)
