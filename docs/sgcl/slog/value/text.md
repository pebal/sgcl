[sgcl](../../README.md) › [slog](../README.md) › [value](README.md)

# sgcl::slog::value::text

```cpp
string text() const;
```

Returns the value as the text handler writes it, not quoted: `5`, `1.5s`, `1e+06`, a time to the millisecond; a value of kind `any` as Go's `%+v` writes it (`[a b]`, `map[k:v]`, `{id:5 path:/a}`); a group as `[k=v k2=v2]`, its values written the same way; `<nil>` for `null`.

## Parameters

None.

## Return value

The text, a new [string](../../core/string/README.md).

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
        println("{}: {}", a.key(), a.value().text());
    }
}
```

Output:

```text
n: 1e+06
took: 1.5s
req: [id=5 tags=[a b]]
none: <nil>
g: [k=1 s=two words]
```

## See also

- [json](json.md)
- [The formats](../README.md#the-formats)
- [sgcl::slog::value](README.md)
