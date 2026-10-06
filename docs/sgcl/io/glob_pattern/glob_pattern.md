[sgcl](../../README.md) › [io](../README.md) › [glob_pattern](README.md)

# sgcl::io::glob_pattern::glob_pattern

```cpp
explicit glob_pattern(const string& pattern, const glob_options& options = {});    // (1)
glob_pattern(const glob_pattern& other) noexcept;                                  // (2), implicitly declared
```

1. The pattern compiled: its braces expanded into its alternatives, each split at its separators and every component
   checked, with the [options](../glob_options.md).
2. The same pattern as `other`, shared.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern ([glob_pattern](README.md) has its syntax) |
| `options` | whether wildcards match hidden names |
| `other` | the pattern this one is |

## Complexity

- (1) Linear in the length of the pattern, times the number of the alternatives of its braces.
- (2) Constant.

## Exceptions

- (1) `bad_expected_access<io::error>` for a malformed pattern (a class that does not close or holds a range
  backwards, a `\` at the end, braces of more than 1024 alternatives); its `error()` is [parse](parse.md)'s,
  `errc::invalid_pattern`. A pattern the program writes is constructed; one from outside is parsed.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::glob_pattern logs("var/log/**/*.log");
    io::glob_pattern same = logs;
    println("{}", same.match("var/log/nginx/access.log"));
    try {
        io::glob_pattern broken("a/[b");
    } catch (const bad_expected_access<io::error>& e) {
        println("{}", e.error().message());
    }
}
```

Output:

```text
true
glob a/[b: invalid pattern
```

## See also

- [parse](parse.md): the same, the error returned
- [sgcl::io::glob_pattern](README.md)
