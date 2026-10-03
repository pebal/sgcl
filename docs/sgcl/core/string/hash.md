[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::hash

```cpp
size_t hash() const noexcept;
```

Returns the hash of the characters: what `std::hash` of the string gives, and what a `map` or a `set` of strings
uses. It is computed the first time it is asked for and kept in the string's object, 32 bits beside the length, as
Java's `String` keeps its `hashCode`: from then on it is a load, and every string that holds the object, a copy
anywhere, finds it there. Strings of the same characters have the same hash, the same object or not. The empty
string's hash is a constant, the same in every run.

The hash is keyed: a hash of the characters under a key of four words drawn once per process, so which texts share a
bucket is not known outside the process, and a map keyed by what a client sends (the names of HTTP headers, the keys
of a JSON object) cannot be filled with keys of one bucket, the attack known as HashDoS. A hash therefore differs
from run to run, and so does the order of a hash container of strings; the environment variable `SGCL_HASH_SEED`, a
number, fixes the key, for a test that prints such an order.

## Parameters

None.

## Return value

The hash of the characters.

## Complexity

Linear in the length the first time, constant after; constant for the empty string.

## Exceptions

None.

## Notes

The object is immutable, and the hash is the one part of it written after it is made: the first thread that asks
stores it, relaxed, and a thread that asks at the same time computes it as well. Every thread computes the same
value, so whichever store lands, the string's hash is the same. The time the hash takes is on
[Benchmarks: Text](../benchmarks.md#text).

A `std::string` key hashes by `std::hash<std::string>`, which is not keyed: a map of untrusted keys is keyed by
`sgcl::string`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string name = "alice";
    string same = name;
    string other("alice");
    println("{} {}", same.hash() == name.hash(), other.hash() == name.hash());
    println("{} {}", std::hash<string>{}(name) == name.hash(), string("bob").hash() == name.hash());
    println("{}", string().hash() == string::hash_of(""));
}
```

Output:

```text
true true
true false
true
```

## See also

- [hash_of](hash_of.md): the hash a string of the given characters has, without making one
- [operator==, operator\<=\>](operator_cmp.md): the equality, which compares the hashes when both are known
- [map](../map/README.md), [set](../set/README.md): the containers a string keys
- [sgcl::string](README.md)
