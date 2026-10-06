[sgcl](../README.md) › [math](README.md)

# sgcl::math::easing

```cpp
#include "sgcl/math/interpolation.h"   // or "sgcl/math.h"

namespace sgcl::math {
    class easing;
}
```

`sgcl::math::easing` is a timing function of an animation: the progress of the animation, 0 at its start and 1 at
its end, to the eased progress that moves the thing animated — slow at first and fast at the end, overshooting and
settling back, bouncing. It is a value of a few numbers, compared with `==` and kept anywhere (an immutable model of a
user interface holds one), called as a function: `math::easing::ease_out_cubic(0.5f)`. CSS's
`animation-timing-function` is the model: [bezier](#member-functions) is its `cubic-bezier()` with the solver
browsers use, [steps](#member-functions) its `steps()`, and its keywords `ease`, `ease-in`, `ease-out`,
`ease-in-out` are constants of the class; so is the named set of easings.net, Robert Penner's equations, spelled
the same way (`ease_in_quad` … `ease_in_out_bounce`).

## Rules

- **The progress is held to [0, 1]**: an animation's own; what comes out may leave it — `back` dips below 0 and
  `elastic` rises above 1, as may a cubic-bezier whose `y1` or `y2` is outside [0, 1]. Every curve but `steps` gives
  exactly 0 at 0 and 1 at 1. NaN in is NaN out.
- **cubic-bezier is solved, not approximated**: the curve's x for the progress by Newton's method from `t = x`,
  halving the interval where the slope is too flat, in double to 1e-13; its `x1` and `x2` are held to [0, 1], as CSS
  requires, so that each progress has one value.
- **steps(count, position)** follows CSS: the progress in `count` equal intervals, the jumps where the
  [step_position](step_position.md) puts them; a count below 1 is one, below 2 for `jump_none`, which needs two.
- **A value.** No pointer, nothing allocated; linear by default.

## Member objects

| Member | Description |
|---|---|
| `linear` | the progress as it is |
| `ease`, `ease_in`, `ease_out`, `ease_in_out` | CSS's keywords: `bezier(0.25, 0.1, 0.25, 1)`, `(0.42, 0, 1, 1)`, `(0, 0, 0.58, 1)`, `(0.42, 0, 0.58, 1)` |
| `ease_in_sine`, `ease_out_sine`, `ease_in_out_sine` | a quarter of a sine wave |
| `ease_in_quad`, `ease_out_quad`, `ease_in_out_quad` | the square of the progress |
| `ease_in_cubic`, `ease_out_cubic`, `ease_in_out_cubic` | its cube |
| `ease_in_quart`, `ease_out_quart`, `ease_in_out_quart` | its fourth power |
| `ease_in_quint`, `ease_out_quint`, `ease_in_out_quint` | its fifth power |
| `ease_in_expo`, `ease_out_expo`, `ease_in_out_expo` | a power of two, 2^(10x − 10) |
| `ease_in_circ`, `ease_out_circ`, `ease_in_out_circ` | a quarter of a circle |
| `ease_in_back`, `ease_out_back`, `ease_in_out_back` | a cubic that pulls back before it goes (by Penner's 1.70158) |
| `ease_in_elastic`, `ease_out_elastic`, `ease_in_out_elastic` | a decaying oscillation |
| `ease_in_bounce`, `ease_out_bounce`, `ease_in_out_bounce` | the bounces of a ball, four arcs |

Each is a `static const easing`; `ease_in_*` starts slowly, `ease_out_*` ends slowly, `ease_in_out_*` does both.

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | linear: the progress as it is |
| `operator()` | `float operator()(float progress) const`: the eased progress |
| `bezier` | `static easing bezier(float x1, float y1, float x2, float y2)`: CSS's `cubic-bezier()`, the curve from (0, 0) to (1, 1) with those control points (named so as not to be taken for the curve type [cubic_bezier](cubic_bezier.md)) |
| `steps` | `static easing steps(int count, step_position position = step_position::jump_end)`: CSS's `steps()` |

## Non-member functions

| Function | Description |
|---|---|
| `operator==` | the same timing function |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    // a box sliding 300 units in an animation, at five moments
    for (float progress : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        float x = math::lerp(0.0f, 300.0f, math::easing::ease_in_out(progress));
        println("{:.2f} {:.1f}", progress, x);
    }
    math::easing custom = math::easing::bezier(0.68f, -0.6f, 0.32f, 1.6f);
    println("{:.4f} {:.4f}", custom(0.2f), math::easing::ease_out_bounce(0.5f));
    println("{} {}", math::easing::steps(4)(0.6f), custom == math::easing::ease);
}
```

Output:

```text
0.00 0.0
0.25 38.7
0.50 150.0
0.75 261.3
1.00 300.0
-0.1046 0.7656
0.5 false
```

## See also

- [step_position](step_position.md): where the jumps of steps() fall
- [lerp](lerp.md): the value at the eased progress
- [cubic_bezier](cubic_bezier.md): a Bézier curve of the plane, which cubic-bezier() is one of
- [README: math](README.md)
