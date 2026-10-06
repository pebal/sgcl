[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::path_of

```cpp
static string path_of(std::initializer_list<string> tokens) noexcept;
```

The JSON Pointer of the tokens, each escaped as RFC 6901 asks (`~` as `~0`, `/` as `~1`): a key from outside
made safe to put in a pointer. No token is `""`, the whole value; an empty token is the empty key, `/`.

## Parameters

| Parameter | Description |
|---|---|
| `tokens` | the keys and indexes, in order |

## Return value

The pointer.

## Complexity

Linear in the length of the tokens.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::json::path_of({"files", "a/b.txt", "0"}));
    auto doc = encoding::json::parse(R"({"files": {"a/b.txt": [7]}})").value();
    println(doc.at_path(encoding::json::path_of({"files", "a/b.txt", "0"}))->to_string());
}
```

Output:

```text
/files/a~1b.txt/0
7
```

## See also

- [at_path](at_path.md), [set_path](set_path.md)
- [sgcl::encoding::json](README.md)
