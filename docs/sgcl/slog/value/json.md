[sgcl](../../README.md) › [slog](../README.md) › [value](../value.md)

# sgcl::slog::value::json

```cpp
string json() const;
```

Returns the value as the JSON handler writes it: `"a"`, `5`, `1000000`, a duration in nanoseconds, a time in RFC 3339 with the nanoseconds; a value of kind `any` as `json.Marshal` writes it; a group as an object, `{"k":1}`; `null` for `null`. NaN and the infinities are slog's error string.

## Parameters

None.

## Return value

The JSON text, a new [string](../../core/string.md).

## Complexity

Linear in the size of the value.

## Exceptions

What a value of the program read through its operations throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

struct request {
    int id = 0;
    vector<string> tags;

    void describe(encoding::field_list& f) {
        f.add("id", id);
        f.add("tags", tags);
    }
};

int main() {
    slog::memory kept;
    slog::logger(kept).info("m", "n", 1e6, "took", 1500 * millisecond,
                            "req", request{5, {"a", "b"}}, "none", nullptr,
                            slog::group("g", "k", 1, "s", "two words"));
    for (auto a : kept.records()[0]) {
        println("{}: {}", a.key(), a.value().json());
    }
}
```

Output:

```text
n: 1000000
took: 1500000000
req: {"id":5,"tags":["a","b"]}
none: null
g: {"k":1,"s":"two words"}
```

## See also

- [text](text.md)
- [The formats](../README.md#the-formats)
- [sgcl::slog::value](../value.md)
