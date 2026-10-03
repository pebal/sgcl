[sgcl](../../README.md) › [slog](../README.md)

# sgcl::slog::handler

```cpp
#include "sgcl/slog/handler.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    class handler;
}
```

`sgcl::slog::handler` is any handler of the program, held as a value: whatever meets
[req::handler](../req/handler.md), a type with `handle(const record&)`, and with `enabled(slog::level)` if it would
rather not be given some levels. A [logger](../logger/README.md) made from one (`slog::logger(h)`, or
[options](../options.md)`::handler`) gives it its records instead of writing lines. It is slog's `Handler` interface
value, with nothing to derive from and nothing virtual: a `tracked_ptr` of what keeps the handler, a pointer to it
and a table of its two methods made once per type, as [io::writer](../../io/writer/README.md) holds a writer.

Go's `Handler` gets a logger's attributes apart (`WithAttrs`, `WithGroup`); here a handler has one method and the
[record](../record/README.md) says it all: the logger's attributes arrive inside it, as a tree.

## Rules

- **What it keeps.** A handler given by `tracked_ptr` is held by it; one of your own given by reference is
  referenced, its managed object kept if it lies in one (one on a stack or a global is yours to keep alive); a
  temporary or a handle of the library ([memory](../memory/README.md)) is copied into a managed object of its own.
- **From every thread.** `handle` is called on the thread that logs, from every thread that logs at once. A handler
  of your own guards what it keeps; `memory` does.
- **A record is a view.** The record, its attributes and their values point at the stack of the call and at the
  objects it named, valid while `handle` runs. [clone](../record/clone.md) is a copy that owns all it holds, to keep
  for later.
- **The tree.** A handler gets the logger's attributes too: `with`'s before a `group` at the top, a logger's
  `group("req")` an attribute of kind group named `req` holding what came after it, and the call's own attributes
  inside the innermost group.
- A logger with a handler is not batched (`options::buffered`): the handler is given each record as it comes.
- A copy is the same handler: the words copied, the handler shared. It lives where a `tracked_ptr` may.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](handler.md) | constructs the handler, empty or holding one of the program |
| `(destructor)` | drops the words |
| `operator=` | copies or moves the words of another handler; the handler moved from holds the same handler still |

#### Operations

| Function | Description |
|---|---|
| [handle](handle.md) | gives a record to the handler held |
| [enabled](enabled.md) | checks whether the handler wants records of a level |

#### Observers

| Function | Description |
|---|---|
| [operator bool](operator_bool.md) | checks whether a handler is held |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct errors_only {
    void handle(const slog::record& r) const {
        println("{}: {}", r.message(), r.size());
        for (auto a : r) {
            println("  {} = {}", a.key(), a.value().text());
        }
    }

    bool enabled(slog::level l) const {
        return l >= slog::level::error;
    }
};

int main() {
    auto log = slog::logger(errors_only()).with("service", "api").group("req");
    log.info("not given to the handler");
    log.error("failed", "id", 7, "ok", false);
}
```

Output:

```text
failed: 2
  service = api
  req = [id=7 ok=false]
```

## See also

- [req::handler](../req/handler.md): what a handler is
- [memory](../memory/README.md): a handler that keeps the records
- [record](../record/README.md), [attr](../attr/README.md), [value](../value/README.md): what a handler reads
- `tests/slog/logger.cpp`: the handlers, `memory`, the record's views and `clone()`, checked
- [sgcl::slog](../README.md)
