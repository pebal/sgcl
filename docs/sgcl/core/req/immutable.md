[sgcl](../../README.md) › [core](../README.md) › [req](../req.md)

# sgcl::req::immutable

```cpp
#include "sgcl/core/req.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class R>
    concept immutable;  // enumerable<R>, and R carries mixin::immutable
}
```

A value that never changes: [enumerable](enumerable.md), and `R` carries
[mixin::immutable](../mixin/immutable.md), a mixin without methods. Every change makes a new container that
shares all but what changed, so a function that takes one may keep it, share it with other threads or compare
it later without a copy.

## Satisfied by

- `immutable::vector`, `immutable::list`, `immutable::map`, `immutable::set`.

Not by `slice<const T>`, which is not written through but may change under it, nor by any mutable container.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

template<req::immutable R>
void remember(vector<R>& history, const R& state) {
    history.push_back(state);  // kept as it is: nothing can change it
}

int main() {
    vector<immutable::vector<int>> history;
    immutable::vector<int> state;
    for (int i : range(3)) {
        state = state.push_back(i);
        remember(history, state);
    }
    println("{} versions, the first {}, the last {}", history.size(), history[0], history[2]);
    println("{}", req::immutable<vector<int>>);
}
```

Output:

```text
3 versions, the first [0], the last [0, 1, 2]
false
```

## See also

- [enumerable](enumerable.md)
- [mixin::immutable](../mixin/immutable.md)
- [immutable](../../immutable/README.md): the module of the immutable containers
- [sgcl::req](../req.md)
