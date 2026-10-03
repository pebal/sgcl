[sgcl](../README.md) › [core](README.md)

# sgcl::variant\<Ts...\>

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class... Ts>
    class variant;

    template<class T> struct variant_size;
    template<class T> inline constexpr size_t variant_size_v = variant_size<T>::value;
    template<size_t I, class T> struct variant_alternative;
    template<size_t I, class T>
    using variant_alternative_t = typename variant_alternative<I, T>::type;

    using std::bad_variant_access;
    using std::monostate;
    using std::variant_npos;
}
```

`sgcl::variant<Ts...>` is `std::variant` for alternatives that hold tracked pointers. `std::variant` keeps every
alternative at the same offset, so a `tracked_ptr` alternative shares its word with the data of the others; the
collector's pointer map, built by elimination, finds data at that offset in some object and drops the offset for
good, and the pointer is no longer followed ([Pointer maps](../../garbage_collector/overview.md#pointer-maps)).
Here the alternatives are laid out by what they hold. The pointer words (`tracked_ptr` of either kind,
[weak_ptr](weak_ptr.md)) share one word that holds null or an address and nothing else: a pointer word's destructor
leaves null behind, so several of them may take the word in turn. An alternative that may hold pointers among its
data (a struct with a `tracked_ptr` member, or anything the collector cannot rule out: a type with a constructor, a
`string` and a `std::string` included) gets a place of its own after it. The alternatives that cannot hold a
pointer (`int`, `double`, a plain struct) share the data storage. The pointer storage starts zeroed, wherever the
variant lives.

The interface is that of `std::variant`: the constructors and their overload resolution
(`variant<int, tracked_ptr<T>> v = make_tracked<T>()` picks the pointer), `in_place_type` and `in_place_index`,
`emplace`, `index`, `valueless_by_exception`, `swap`, [get](variant/get.md), [get_if](variant/get_if.md),
[holds_alternative](variant/holds_alternative.md), [visit](variant/visit.md), `monostate`, `bad_variant_access`,
`variant_npos`, `variant_size`, `variant_alternative` (also as specializations of the `std` traits), the six
comparisons and `<=>` when the alternatives have them, `std::hash` when they do. What differs: nothing is
`constexpr` (the alternatives live in raw storage), the variant is never trivially copyable, and its size is the
pointer word plus the places of the alternatives that may hold pointers plus the largest of the others
(`variant<int, tracked_ptr<T>, weak_ptr<T>>` is 16 bytes). A variant of pointer-free alternatives has no reason to
be one of these: `std::variant` is smaller and `constexpr`. Go has no sum type; an interface with a type switch is
its nearest form.

## Rules

- The variant has no word of its own. Where it may live is decided by its alternatives: with `tracked_ptr` or
  `weak_ptr` alternatives, or alternatives that hold them, where a `tracked_ptr` may, on a stack or inside a
  managed object. An `sgcl::tracked_ptr` alternative in a variant on the unmanaged heap is the same mistake as an
  `sgcl::tracked_ptr` in a `std::vector` ([The rules](README.md#the-rules), 1).
- An alternative that may hold pointers is traced through the pointer map of the object holding the variant, like
  a member at the same offset would be; its own data words leave the map by elimination, its pointer words never
  do.
- A pointer alternative is destroyed when another is emplaced or assigned, or the variant is destroyed: its object
  is unreferenced from then on, and dies with the next cycle that finds it so.
- In a destructor, a pointer alternative is a `tracked_ptr` member: not to be read there
  ([The rules](README.md#the-rules), 5).
- Thread safety is that of `std::variant`: threads share one with the program's own synchronization
  ([The rules](README.md#the-rules), 6).

## Template parameters

| Parameter | Description |
|---|---|
| `Ts` | The alternatives, at least one: object types that are not arrays and are destructible. A pointer word (`tracked_ptr`, `weak_ptr`) goes into the shared word, a type that may hold pointers into a place of its own, any other into the shared data storage. |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](variant/variant.md) | constructs the variant |
| `(destructor)` | destroys the alternative held; a pointer leaves null in its word |
| [operator=](variant/operator_assign.md) | assigns another variant or a value |

#### Observers

| Function | Description |
|---|---|
| [index](variant/index.md) | the index of the alternative held |
| [valueless_by_exception](variant/valueless_by_exception.md) | checks whether the variant holds nothing after a construction threw |

#### Modifiers

| Function | Description |
|---|---|
| [emplace](variant/emplace.md) | constructs an alternative in place |
| [swap](variant/swap.md) | swaps the contents of two variants |

## Non-member functions

| Function | Description |
|---|---|
| [visit](variant/visit.md) | calls a function with the alternatives held by one or more variants |
| [holds_alternative](variant/holds_alternative.md) | checks whether the variant holds a given alternative |
| [get](variant/get.md) | the alternative by index or by type, `bad_variant_access` on another |
| [get_if](variant/get_if.md) | a pointer to the alternative by index or by type, null on another |
| [operator==, operator!=, operator\<, operator\<=, operator\>, operator\>=, operator\<=\>](variant/operator_cmp.md) | compare two variants |
| [swap](variant/swap2.md) | swaps the contents of two variants |

#### Helper classes and objects

| Name | Description |
|---|---|
| `variant_size`, `variant_size_v` | the number of alternatives of a variant type, `const` or not |
| `variant_alternative`, `variant_alternative_t` | the type of the alternative at an index, `const` with a `const` variant |
| `monostate` | `std::monostate`: an empty alternative, the first one of a variant that is default constructible otherwise |
| `bad_variant_access` | `std::bad_variant_access`: what `get` and `visit` throw |
| `variant_npos` | `std::variant_npos`: the `index()` of a valueless variant |

## Specializations

`std::variant_size<sgcl::variant<Ts...>>` and `std::variant_alternative<I, sgcl::variant<Ts...>>` give what the
library's traits give, so generic code that asks `std` finds the alternatives. `std::hash<sgcl::variant<Ts...>>`
takes part when every alternative has a `std::hash`: the hash of the alternative held, mixed with its index; a
valueless variant hashes to a constant.

## Complexity

Every operation is constant, plus the operation of the alternative it runs (a copy, a construction, a
comparison). An operation on whichever alternative is held goes through a table of one function per index, one
indirect call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

// A tree of values: a leaf holds a number or a name, a branch its children. The children are
// tracked pointers, next to the data, in one variant per node: the collector follows them.
struct Node;
using Children = vector<tracked_ptr<Node>>;
struct Node {
    variant<double, string, Children> value;
};

double sum(const tracked_ptr<Node>& node) {
    return visit([](const auto& v) -> double {
        using T = std::remove_cvref_t<decltype(v)>;
        if constexpr (std::is_same_v<T, double>) {
            return v;
        } else if constexpr (std::is_same_v<T, Children>) {
            double total = 0;
            for (auto& child : v) {
                total += sum(child);
            }
            return total;
        } else {
            return 0;
        }
    }, node->value);
}

int main() {
    tracked_ptr root = make_tracked<Node>();
    Children children;
    children.push_back(make_tracked<Node>(1.5));
    children.push_back(make_tracked<Node>(string("name")));
    children.push_back(make_tracked<Node>(2.5));
    root->value = std::move(children);
    println("{}", sum(root));

    root->value = 0.0;  // the children unreferenced: collected
    collector::force_collect(true);  // optional, for the demonstration only
    println("{}", sum(root));
}
```

Output:

```text
4
0
```

## See also

- [any](any.md): the same for a value of any type
- [expected](expected.md): a value or an error, over a variant
- [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md): the pointer words
- [Pointer maps](../../garbage_collector/overview.md#pointer-maps), [README: The rules](README.md#the-rules)
