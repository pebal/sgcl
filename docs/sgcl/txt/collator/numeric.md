[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::numeric

```cpp
bool numeric() const noexcept;
```

Returns whether the collator compares a run of digits as the number it spells, CLDR's `kn`: `plik9` before
`plik10`. No language asks for it, so it is what the [options](../collator-options.md) said, `false` unless they set it.

## Parameters

None.

## Return value

`true` when runs of digits are compared as numbers, `false` otherwise.

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
    vector<string> files = {"plik10.txt", "plik9.txt", "plik007.txt", "plik100.txt"};
    txt::collator numbers{{.numeric = true}};
    files.sort(numbers);
    println("{} {}", numbers.numeric(), files);
    println("{}", numbers.equal("007", "7"));
}
```

Output:

```text
true ["plik007.txt", "plik9.txt", "plik10.txt", "plik100.txt"]
true
```

## See also

- [options](../collator-options.md)
- [sgcl::txt::collator](../collator.md)
