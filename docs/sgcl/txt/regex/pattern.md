[sgcl](../../README.md) › [txt](../README.md) › [regex](../regex.md)

# sgcl::txt::regex::pattern

```cpp
const string& pattern() const noexcept;
```

Returns the pattern the regex was compiled from, as it was written: a regex keeps it, since the names of its groups
are read from it.

## Parameters

None.

## Return value

The pattern.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string typed = "(?i)kot\\w*";
    auto re = txt::regex::compile(typed);
    println("{} finds {}", re->pattern(), re->find("Ala ma Kotka")->text());
}
```

Output:

```text
(?i)kot\w* finds Kotka
```

## See also

- [compile](compile.md): a pattern that arrives while the program runs
- [sgcl::txt::regex](../regex.md)
