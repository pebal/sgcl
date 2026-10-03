[sgcl](../../README.md) › [core](../README.md) › [req](../req.md)

# sgcl::req::handle

```cpp
#include "sgcl/core/atomic.h"   // or "sgcl/core.h"

namespace sgcl::req {
    template<class H>
    concept handle;  // a public type of one tracked word to the object inside it
}
```

A handle of the library: a public type that is one tracked word to the object inside it, whose copies share
that object. `atomic<H>` and `atomic_ref<H>` are specialized for it, over that word, so that a handle replaced by
one thread while others read it is one word, loaded and stored without a copy and compared by identity.

## Satisfied by

- `string`, `io::file`, `io::buffer`, `net::connection`, `async::channel`, `async::mutex`, and every other
  public type of the library that holds its object by one tracked word.

Not by `tracked_ptr` (which has atomics of its own), `slice` (two more words), or `int`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

template<req::handle H>
H replace(atomic<H>& current, const H& next) {
    return current.exchange(next);
}

int main() {
    atomic<string> host = string("old.example");
    string previous = replace(host, string("new.example"));
    println("{} -> {}", previous, host.load());
    println("{} {} {}", req::handle<string>, req::handle<tracked_ptr<int>>, req::handle<int>);
}
```

Output:

```text
old.example -> new.example
true false false
```

## See also

- [atomic](../atomic.md), [atomic_ref](../atomic_ref.md): the atomics over the word of a handle
- [rooted](../rooted.md): a handle kept where a tracked word may not live
- [sgcl::req](../req.md)
