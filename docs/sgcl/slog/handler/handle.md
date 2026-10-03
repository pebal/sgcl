[sgcl](../../README.md) › [slog](../README.md) › [handler](README.md)

# sgcl::slog::handler::handle

```cpp
void handle(const record& r) const;
```

Gives the record `r` to the handler held: its `handle(r)`, through the table made for its type. A logger calls it
for every record at its level that the handler's [enabled](enabled.md) took, on the thread that logs. An empty
handler throws `logic_error`.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the record, a view valid during the call |

## Return value

None.

## Complexity

One indirect call, then what the handler's own `handle` costs.

## Exceptions

- What the handler's own `handle` throws.
- `logic_error` when no handler is held: `sgcl::slog::handler: no handler is held`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct printer {
    void handle(const slog::record& r) const {
        println("{} with {} attributes", r.message(), r.size());
    }
};

int main() {
    slog::handler h = printer();
    h.handle(slog::record());
    slog::logger(h).info("from a logger", "a", 1, "b", 2);
}
```

Output:

```text
 with 0 attributes
from a logger with 2 attributes
```

## See also

- [enabled](enabled.md)
- [record](../record/README.md): what a handler reads
- [sgcl::slog::handler](README.md)
