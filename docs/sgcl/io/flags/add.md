[sgcl](../../README.md) › [io](../README.md) › [flags](README.md)

# sgcl::io::flags::add

```cpp
template<class T>
void add(const string& name, T& target, const string& help);
```

Adds the flag `-name`, tied to the variable `target`, with a line of help for the usage: Go's `flag.IntVar`,
`flag.StringVar` and the rest in one name. The value of `target` now is the flag's default, which the usage shows
unless it is the type's zero value. Takes part only when `T` is `bool`, an integer, a floating-point number,
`string`, [duration](../../core/duration/README.md), or a type with `T::parse(const string&)` returning an `expected` of a
value `T` is made from (Go's `flag.Var`); the usage writes the default of such a type by its `to_string()`.

The type's name in the usage is `int`, `uint`, `float`, `string`, `duration` or `value`, none for a bool, unless a
word of the help stands in backquotes, which names it instead.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the flag, without the `-`: not empty, not beginning with `-`, without `=` |
| `target` | the variable the flag sets; it outlives the parse |
| `help` | the line of help in the usage |

## Return value

None.

## Complexity

Linear in the number of flags added so far: the name is checked against them.

## Exceptions

`std::invalid_argument` when the name is empty, begins with `-` or holds `=`, and when a flag of the name was added
already (Go panics): the program's error, with the message `sgcl::io::flags: flag redefined: port` or
`sgcl::io::flags: a flag's name may be neither empty nor begin with - nor hold =: -x`. What `to_string()` of a type
of the program's throws.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    int count = 1;
    unsigned mask = 0;
    double ratio = 0.5;
    string name = "anon";
    duration every = duration::zero();
    bool dry = false;
    io::flags f;
    f.add("count", count, "how many");
    f.add("mask", mask, "the bits");
    f.add("ratio", ratio, "the ratio");
    f.add("name", name, "who");
    f.add("every", every, "how often");
    f.add("n", dry, "dry run");
    auto r = f.parse(vector<string>{"-count=1_000", "-mask", "0x1F", "-ratio=0x1p-2", "--name=ada",
                                    "-every=1m30s", "-n"});
    println("{} {} {} {} {} {} {}", bool(r), count, mask, ratio, name, every, dry);
    try {
        f.add("count", count, "again");
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true 1000 31 0.25 ada 1m30s true
sgcl::io::flags: flag redefined: count
```

## See also

- [positional](positional.md): the arguments after the flags
- [parse](parse.md): reads the command line into the variables
- [sgcl::io::flags](README.md)
