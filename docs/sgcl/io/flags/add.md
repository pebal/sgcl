[sgcl](../../README.md) › [io](../README.md) › [flags](README.md)

# sgcl::io::flags::add

```cpp
template<class T>
void add(const string& name, T& target, const string& help);                                 // (1)
template<class T>
void add(const string& name, T& target, const string& help, const flag_options& options);    // (2)
template<class T>
void add(const string& name, vector<T>& target, const string& help,                          // (3)
         const flag_options& options = {});
void add(const flag& f);                                                                     // (4)
```

Adds the flag `-name`, tied to the variable `target`, with a line of help for the usage.

1. Go's `flag.IntVar`, `flag.StringVar` and the rest in one name.
2. The same with what Go has not ([flag_options](../flag_options.md)): a second name (`-p` beside `-port`), the
   variable of the environment whose value is the default when it is set and not empty, read at the parse before
   the command line, which wins, and a flag the command line is refused without.
3. A list: every occurrence of the flag appends a value of `T` to the vector, the first occurrence of a parse
   replacing what it held, its default; a value of the environment is split at commas. A list of `bool` takes
   `-x` alone as `true`, as a bool does. The usage writes `...` after the type.
4. The flag of an [io::flag](../flag.md), the value the one-line forms are lists of.

- (1–3) The value of `target` now is the flag's default, which the usage shows
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
| `options` | the second name, the variable of the environment, whether the flag is required |
| `f` | a flag as a value |

## Return value

None.

## Complexity

Linear in the number of flags added so far: the names are checked against theirs.

## Exceptions

`std::invalid_argument` when the name or the second name is empty, begins with `-` or holds `=`, and when a flag of
either name was added already, or the two are one (Go panics): the program's error, with the message `sgcl::io::flags: flag redefined: port` or
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

A second name, a list and the environment:

```cpp
#include "sgcl/io.h"
#include <cstdlib>

using namespace sgcl;

int main() {
    ::setenv("APP_PORT", "9000", 1);
    int port = 8080;
    vector<string> tags;
    io::flags f;
    f.add("port", port, "the port", {.short_name = "p", .env = "APP_PORT"});
    f.add("tag", tags, "a tag", {.short_name = "t"});
    f.parse(vector<string>{"-t", "a", "--tag=b"});
    println("{} {} {}", port, tags[0], tags[1]);
    f.parse(vector<string>{"-p", "1"});
    println("{}", port);
    vector<string> lines(f.usage().split('\n'));
    for (const string& line : lines.as_slice(1)) {
        if (!line.empty()) {
            println("{}", line);
        }
    }
}
```

Output:

```text
9000 a b
1
  -p, -port int
    	the port (default 8080) [$APP_PORT]
  -t, -tag string...
    	a tag
```

## See also

- [flag_options](../flag_options.md): what (2) and (3) take
- [positional](positional.md): the arguments after the flags
- [parse](parse.md): reads the command line into the variables
- [sgcl::io::flags](README.md)
