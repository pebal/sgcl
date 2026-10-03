[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::req

```cpp
#include "sgcl/slog/handler.h"   // or "sgcl/slog.h"

namespace sgcl::slog::req {
    template<class T> concept handler;
}
```

The requirements of slog (`namespace sgcl::slog::req`) say what the module takes from the program: a handler of
records, whatever has `handle(const record&)`. As with io's streams ([io::req](../io/req.md)), there is no base
class and nothing virtual: a class of your own is a handler by having the method, the concept is checked where one
is given, and [handler](handler.md) holds any of them as a value where one has to be kept.

## Requirements

| Requirement | Description |
|---|---|
| [handler](req/handler.md) | a type with `handle(const record&)`, and `enabled(level)` if it has one |

## See also

- [handler](handler.md): any handler, as a value
- [sgcl::slog](README.md)
