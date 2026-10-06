[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::parse_all

```cpp
static expected<vector<yaml>, error> parse_all(const string& text) noexcept;                      // (1)
static expected<vector<yaml>, error> parse_all(const string& text, const options& o) noexcept;    // (2)
```

Every document of the text, in order: the documents of a stream separated by `---` (a log, a Kubernetes manifest of
several objects); none for a text of nothing but comments.

1. With the default [options](../yaml-options.md).
2. With the options given.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text |
| `o` | what is accepted |

## Return value

The documents, or the [error](../error/README.md) of the first that is not one, with its line and column.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto docs = encoding::yaml::parse_all("kind: Service\n---\nkind: Deployment\n...\n--- # empty\n");
    for (const auto& d : docs.value()) {
        println(d["kind"].as_string("(none)"));
    }
}
```

Output:

```text
Service
Deployment
(none)
```

## See also

- [parse](parse.md)
- [sgcl::encoding::yaml](README.md)
