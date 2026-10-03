[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::operator()

```cpp
bool operator()(const string& a, const string& b) const noexcept;
```

Checks whether `a` comes before `b`, `compare(a, b) < 0`: so that a collator may stand wherever a comparator is asked
for — `std::sort`, the `sort` of a container, the `Compare` of a [sorted_map](../../core/sorted_map/README.md), whose call
must be noexcept, and this one is.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the texts, UTF-8 |

## Return value

`true` when `a` comes first, `false` otherwise.

## Complexity

Linear in the lengths of the texts at worst; a difference in the first letters is found at once.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    sorted_map<string, int, txt::collator> population(txt::collator(txt::locale("pl")));
    population["Łódź"] = 655;
    population["Lublin"] = 333;
    population["Zielona Góra"] = 139;
    population["Żory"] = 62;
    for (const auto& [city, thousands] : population) {
        print("{} ", city);
    }
    println();
}
```

Output:

```text
Lublin Łódź Zielona Góra Żory 
```

## See also

- [compare](compare.md): the order of two texts
- [key](key.md): the order as bytes
- [sgcl::txt::collator](README.md)
