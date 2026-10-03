[sgcl](../../README.md) › [slog](../README.md) › [group](README.md)

# sgcl::slog::group\<A...\>::group

```cpp
group(const char* name, const A&... kv) noexcept;
```

Constructs a group named `name` of the pairs `kv`, deduced from the arguments: `slog::group("req", "id", id,
"path", path)`. It refers to `name` and to every argument; nothing is copied or rendered until the record or the
`with` that takes it does so.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the group; an empty or a null one is no group, its attributes standing where it stands |
| `kv` | the pairs inside, a key then a value, and groups; what a verb takes |

## Complexity

Linear in the length of `name`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::logger log(io::stdout);
    string path = "/users";
    log.info("served", slog::group("http", "path", path, "status", 200), "took", 3 * millisecond);
    log.info("unnamed", slog::group(nullptr, "a", 1));
}
```

Output:

```text
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=served http.path=/users http.status=200 took=3ms
time=2026-09-28T14:05:01.123+02:00 level=INFO msg=unnamed a=1
```

## See also

- [sgcl::slog::group](README.md)
