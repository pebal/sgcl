[sgcl](../../README.md) › [io](../README.md) › [glob_pattern](README.md)

# sgcl::io::glob_pattern::parse

```cpp
static expected<glob_pattern, error> parse(const string& pattern, const glob_options& options = {}) noexcept;
```

The pattern compiled, a malformed one an error, where the [constructor](glob_pattern.md) throws it: for a pattern that comes from outside — a command line, a configuration file, a request.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern ([glob_pattern](README.md) has its syntax) |
| `options` | whether wildcards match hidden names |

## Return value

The pattern, or the [error](../error/README.md) `errc::invalid_pattern`, its operation `glob` and its path the pattern.

## Complexity

Linear in the length of the pattern, times the number of the alternatives of its braces.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* text : {"*.{jpg,png}", "[z-a]*", "x\\"}) {
        auto p = io::glob_pattern::parse(text);
        println("{}", p ? "compiled" : p.error().message());
    }
}
```

Output:

```text
compiled
glob [z-a]*: invalid pattern
glob x\: invalid pattern
```

## See also

- [(constructor)](glob_pattern.md): the same, the error thrown
- [sgcl::io::glob_pattern](README.md)
