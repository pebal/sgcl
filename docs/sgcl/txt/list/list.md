[sgcl](../../README.md) › [txt](../README.md) › [list](../list.md)

# sgcl::txt::list::list

```cpp
/*(1)*/ list() noexcept;
/*(2)*/ list(std::initializer_list<value> items) noexcept;
/*(3)*/ explicit list(const vector<value>& items) noexcept;
```

1. An empty list, written `[]`: `txt::list{}` as well.
2. A list of `items`, in order; a nested list or mapping is written in braces of its own, `txt::list{1, {2, 3}}`.
3. A list of the elements of `items`, for a list whose length is not known where the program is written; an empty
   vector makes an empty list, written `[]`.

## Parameters

| Parameter | Description |
|---|---|
| `items` | the elements |

## Complexity

- (1) Constant: one allocation, the empty list's.
- (2–3) Linear in the number of elements.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    vector<txt::value> rows;
    for (int i : range(3)) {
        rows.push_back(i * i);
    }
    txt::value squares = txt::list(rows);
    txt::value nested = txt::list{1, {2, 3}};
    txt::value no_elements = txt::list{};
    txt::value empty = txt::list(vector<txt::value>());
    println("{} {} {} {}", squares, nested, no_elements, empty);
    return 0;
}
```

Output:

```text
[0, 1, 4] [1, [2, 3]] [] []
```

## See also

- [sgcl::txt::list](../list.md)
