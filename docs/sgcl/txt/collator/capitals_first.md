[sgcl](../../README.md) › [txt](../README.md) › [collator](../collator.md)

# sgcl::txt::collator::capitals_first

```cpp
bool capitals_first() const noexcept;
```

Returns whether the collator settled on putting the capitals before the small letters,
[case_order::upper_first](../case_order.md), CLDR's `caseFirst` (`kf`). It is the caller's answer where the
[options](../collator-options.md) gave one, the language's where they did not — Danish, Maltese and Church Slavonic ask for
it.

## Parameters

None.

## Return value

`true` when the capitals come first, `false` otherwise.

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
    for (const char* tag : {"pl", "da", "mt", "cu"}) {
        print("{}:{} ", tag, txt::collator(txt::locale(tag)).capitals_first());
    }
    println();
}
```

Output:

```text
pl:false da:true mt:true cu:true 
```

## See also

- [case_order](../case_order.md), [options](../collator-options.md)
- [sgcl::txt::collator](../collator.md)
