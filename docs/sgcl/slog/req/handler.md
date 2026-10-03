[sgcl](../../README.md) › [slog](../README.md) › [req](../req.md)

# sgcl::slog::req::handler

```cpp
#include "sgcl/slog/handler.h"   // or "sgcl/slog.h"

namespace sgcl::slog::req {
    template<class T>
    concept handler;   // h.handle(r) on a const h, r a const record&
}
```

`handler` is what takes records, slog's `Handler`: `h.handle(r)` called on a `const` object of the type with a
`const record&`. A type may have `enabled(slog::level)` as well, convertible to `bool`, asked before a record is made;
without one it is given every level. `T` is the type as passed, references and `const` looked through.

## Satisfied by

- [memory](../memory.md);
- a class of the program with `void handle(const slog::record&) const`.

Not by a class whose `handle` is not `const`, a lambda (it has no `handle`), or a `tracked_ptr` to a handler: a
[handler](../handler.md) is constructed from such a pointer, but the concept asks of the type itself.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct printer {
    void handle(const slog::record& r) const {
        println("{}", r.message());
    }
};

struct not_const {
    void handle(const slog::record&) {
    }
};

int main() {
    println("{} {} {}", slog::req::handler<printer>, slog::req::handler<slog::memory>,
            slog::req::handler<not_const>);
    slog::logger(printer()).info("printed by the handler");
}
```

Output:

```text
true true false
printed by the handler
```

## See also

- [handler](../handler.md): any handler, as a value
- [sgcl::slog::req](../req.md)
