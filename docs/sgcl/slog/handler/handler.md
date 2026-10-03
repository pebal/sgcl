[sgcl](../../README.md) › [slog](../README.md) › [handler](README.md)

# sgcl::slog::handler::handler

```cpp
handler() noexcept = default;                // (1)
template<class H>
handler(H&& h) noexcept(/* see below */);    // (2)
```

Constructs a handler.

1. An empty handler: `bool(h)` is `false`, and its [handle](handle.md) and [enabled](enabled.md) throw `logic_error`.
   A logger is never made from one: [options](../options.md)`::handler` empty means lines on `out`.
2. A handler of `h`: any type that meets [req::handler](../req/handler.md), or a `tracked_ptr` to one; not a
   `handler` itself, which is copied. Not explicit: a handler of the program converts wherever one is taken.
   What is kept depends on `h`:
   - a `tracked_ptr`: held by it;
   - a handler of the program given by reference: referenced, with the managed object it lies in kept, if it lies
     in one; one on a stack or a global is the caller's to keep alive;
   - a temporary of the program's, or a handle of the library ([memory](../memory/README.md)) however given:
     copied or moved into a managed object of its own.

## Parameters

| Parameter | Description |
|---|---|
| `h` | the handler to hold |

## Complexity

Constant: one managed allocation for a temporary or a handle of the library, none otherwise.

## Exceptions

- (1) None.
- (2) None, but for a temporary or a handle of the library: what its copy or move into the managed object
  throws; none when that is noexcept, and so the constructor is.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct counter {
    mutable int seen = 0;

    void handle(const slog::record&) const {
        ++seen;
    }
};

int main() {
    slog::handler empty;
    counter mine;
    slog::handler by_reference = mine;  // mine is referenced: it must outlive the logger
    sgcl::tracked_ptr kept = make_tracked<counter>();
    slog::handler by_pointer = kept;

    slog::logger(by_reference).info("one");
    slog::logger(by_pointer).info("two");
    slog::logger(by_pointer).info("three");
    println("{} {} {}", bool(empty), mine.seen, kept->seen);
}
```

Output:

```text
false 1 2
```

## See also

- [operator bool](operator_bool.md)
- [io::writer::writer](../../io/writer/writer.md): the same rules for a writer
- [sgcl::slog::handler](README.md)
