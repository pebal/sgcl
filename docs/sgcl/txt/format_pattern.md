[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_pattern\<A...\>

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    template<class... A>
    class format_pattern;
}
```

The pattern of [format](format.md), read where the program is compiled. A literal turns into one on its way into
`format`, [format_to](format_to.md), [io::print](../io/print.md) or a function of the program that takes one, and the
reading happens there, in a `consteval` constructor that knows the types of the values: a brace left open, a number
with no value behind it, a precision asked of a whole number — each is an error of the compiler, with the pattern in
hand. A pattern that is not a constant does not come this way at all, which is deliberate: it is a
[runtime_pattern](runtime_pattern.md).

The same pass that checks the pattern writes down its steps — the run of literal text, the value that follows it,
the specification already made out — so a call walks over as many steps as the pattern has fields, not over its
characters. A pattern keeps four steps, which covers a message; one with more, or with a number too large for the
narrow fields of a step (a width past 65535, a precision past 32767, a pattern longer than 65535 characters), keeps
none and is read where it runs, which is correct and only a little slower. `std::format` checks its pattern where the
program is compiled too (`std::format_string`), and reads it again at every call; Go's `fmt` checks nothing before
the program runs, but `go vet`.

## Rules

- It is made only where the program is compiled: its constructor is `consteval`, so the text is a constant, a literal
  or a `constexpr` view. It holds a view of that text and the steps, nothing managed.
- A function of the program that takes a checked pattern names it as `format` does,
  `const txt::format_pattern<std::type_identity_t<A>...>&` beside `const A&... args`: the types are deduced from the
  values alone, and the literal converts.

## Template parameters

| Parameter | Description |
|---|---|
| `A...` | the types of the values the pattern is checked against, as the call passes them |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](format_pattern/format_pattern.md) | reads and checks the pattern, where the program is compiled |

#### Observers

| Function | Description |
|---|---|
| [view](format_pattern/view.md) | the text of the pattern |
| [parts](format_pattern/parts.md) | the steps the pattern was read into |
| [count](format_pattern/count.md) | how many steps; `0` for a pattern read where it runs |

## Example

A function of one's own whose pattern the compiler checks:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

template<class... A>
void report(const txt::format_pattern<std::type_identity_t<A>...>& pattern, const A&... args) {
    println("report: {}", txt::format(pattern, args...));
}

int main() {
    report("{} of {} done", 3, 10);
    report("{:>6.1f}%", 30.0);
    // report("{:d}", "thirty");  // does not compile: a name is not a number
    return 0;
}
```

Output:

```text
report: 3 of 10 done
report:   30.0%
```

## See also

- [format](format.md): the rules of the pattern
- [runtime_pattern](runtime_pattern.md): a pattern read where the program runs
- [format_part](format_part.md): a step of a pattern
- [print](../io/print.md): takes one
