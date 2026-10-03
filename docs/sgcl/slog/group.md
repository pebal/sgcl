[sgcl](../README.md) › [slog](README.md)

# sgcl::slog::group\<A...\>

```cpp
#include "sgcl/slog/logger.h"   // or "sgcl/slog.h"

namespace sgcl::slog {
    template<class... A>
    class group;
}
```

`sgcl::slog::group` is attributes in a group of their own, slog's `Group`: one argument of a record or of
[with](logger/with.md), standing where a key would, with its name inside.
`log.info("m", slog::group("req", "id", 5))` writes `req.id=5` as text and `"req":{"id":5}` as JSON. The pairs
inside are what a verb takes, groups too, and are checked by the compiler the same way. To put every later
attribute of a logger in a group, [logger::group](logger/group.md) is the method.

## Rules

- A group of no attributes, or of empty groups only, is left out, as slog leaves it.
- Groups nest without a limit: every level the program writes is written, in both formats.
- A group without a name (an empty or a null one) puts its attributes where it stands.
- A group given as the value of a key is an error of the build: it stands on its own, with its name inside.
- It keeps its arguments by reference, as the call does: it is made in the call that logs it, never kept.
  [with](logger/with.md) copies what a group holds, so a logger made with one may outlive the arguments.

## Template parameters

| Parameter | Description |
|---|---|
| `A` | the types of the pairs inside, deduced from the constructor's arguments |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](group/group.md) | constructs the group from its name and its pairs |

## Deduction guides

```cpp
template<class... A>
group(const char*, const A&...) -> group<A...>;
```

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger text(io::stdout);
    slog::logger json(slog::options{.out = io::stdout, .json = true});
    for (const auto& log : {text, json}) {
        log.info("request", slog::group("req", "id", 7, slog::group("peer", "tls", true)),
                 slog::group("empty"), slog::group("", "flat", 1));
    }
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=request req.id=7 req.peer.tls=true flat=1
{"time":"2026-09-28T14:05:01.123456+02:00","level":"INFO","msg":"request","req":{"id":7,"peer":{"tls":true}},"flat":1}
```

## See also

- [logger::group](logger/group.md): every later attribute of a logger in a group
- [logger::with](logger/with.md): attributes in every record
- [sgcl::slog](README.md)
