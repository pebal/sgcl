[sgcl](../../README.md) › [net](../README.md) › [query_params](README.md)

# sgcl::net::query_params::query_params

```cpp
query_params() noexcept = default;              // (1)
explicit query_params(const string& text);      // (2)
query_params(const query_params& other);        // (3)
query_params(query_params&& other) noexcept;    // (4)
```

Constructs a list of pairs.

1. An empty list.
2. The pairs a literal in the program spells: what [parse](parse.md) reads, or `bad_expected_access<io::error>` with
   `parse`'s error. `parse` stays for a text from outside the program.
3. A copy of the pairs of `other`, a list of its own.
4. The pairs of `other`, which is left empty, as a [vector](../../core/vector/README.md) moved from is; the assignments copy
   and move the same way.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the query or the form body, with a leading `?` or without it |
| `other` | the list to copy or move from |

## Complexity

- (1) Constant.
- (2) Linear in the length of `text`.
- (3) Linear in the length of the pairs.
- (4) Constant.

## Exceptions

- (1), (3), (4) None.
- (2) `bad_expected_access<io::error>` when `text` is past [the limit](README.md#rules) of 512 MiB, or its
  pairs would be written past it; its `error()` is `parse`'s, of the code `net::errc::invalid_url`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::query_params none;
    net::query_params defaults("lang=pl&page=1");
    println("{} {}", none.size(), defaults.to_string());
}
```

Output:

```text
0 lang=pl&page=1
```

## See also

- [parse](parse.md): reads a text from outside the program
- [sgcl::net::query_params](README.md)
