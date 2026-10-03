[sgcl](../README.md) › [core](README.md) › [collector](collector/README.md) › [stepper](collector-stepper/README.md)

# sgcl::collector::stepper::phase

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        class stepper {
        public:
            enum class phase : int { start, flipped, registered, roots, marked, swept, released };
        };
    };
}
```

`sgcl::collector::stepper::phase` is a gate of the collector: a boundary between the phases of a cycle, where a
[stepper](collector-stepper/README.md) holds the collector until it lets it through. The gates come in this order in
every cycle.

| Value | Description |
|---|---|
| `start` | a cycle about to begin |
| `flipped` | the epoch flipped, nothing registered yet: an object made now stays unregistered for this cycle |
| `registered` | the pages, objects and threads of before the flip registered, the blocks of cells released |
| `roots` | the stacks scanned, the dirty pages of a young cycle traced |
| `marked` | the marking converged, the weak cells cleared |
| `swept` | the garbage destroyed and freed |
| `released` | the empty pages back in the heap and the cycle in the [statistics](collector-statistics.md): the cycle is over |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

const char* name_of(collector::stepper::phase p) {
    using enum collector::stepper::phase;
    switch (p) {
        case start: return "start";
        case flipped: return "flipped";
        case registered: return "registered";
        case roots: return "roots";
        case marked: return "marked";
        case swept: return "swept";
        case released: return "released";
    }
    return "";
}

int main() {
    collector::stepper s;
    s.finish_cycle();
    for (int i : range(7)) {
        print("{} ", name_of(s.step()));
    }
    println();
}
```

Output:

```text
start flipped registered roots marked swept released 
```

## See also

- [stepper](collector-stepper/README.md): what stands at the gates
- [sgcl::collector](collector/README.md)
