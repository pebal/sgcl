[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::tailored

```cpp
bool tailored() const noexcept;
```

Returns whether the library has an order of its own for the language of the collator's locale, one of the 88 in
the tables ([collator](README.md#the-root-order-and-a-languages-own)). Where it has none, the collator puts
the text in the root order, which is an answer rather than a failure: English, French, Italian, Dutch and German
ask for nothing the root order does not already do.

## Parameters

None.

## Return value

`true` when the language has a tailoring here, `false` for the root order.

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
    for (const char* tag : {"pl", "de", "tlh", "sr-Latn"}) {
        print("{}:{} ", tag, txt::collator(txt::locale(tag)).tailored());
    }
    println();
}
```

Output:

```text
pl:true de:false tlh:false sr-Latn:false 
```

## See also

- [where](where.md): the locale
- [sgcl::txt::collator](README.md)
