# sgcl::variant

```cpp
#include "sgcl/core/variant.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class... Ts>
    class variant;
}
```

`sgcl::variant<Ts...>` is `std::variant` for alternatives that hold tracked pointers. `std::variant` keeps every alternative at the same offset, so a `tracked_ptr` alternative shares its word with the data of the others; the collector's pointer map, built by elimination, finds data at that offset in some object and drops the offset for good, and the pointer is no longer followed ([README: Pointer maps](../../garbage_collector/overview.md#pointer-maps)). Here the alternatives are laid out by what they hold: the pointer words (`tracked_ptr` of either kind, [`weak_ptr`](weak_ptr.md)) share one word that holds null or an address and nothing else (a pointer word's destructor leaves null behind, so several of them may take the word in turn); an alternative that may hold pointers among its data (a struct with a `tracked_ptr` member, or anything the collector cannot rule out: a type with a constructor, `std::string` included) gets a place of its own; the alternatives that cannot hold a pointer (`int`, `double`, a plain struct) share the data storage. The pointer storage starts zeroed, wherever the variant lives.

The interface is that of `std::variant`: the constructors and their overload resolution (`variant<int, tracked_ptr<T>> v = make_tracked<T>()` picks the pointer), `in_place_type` and `in_place_index`, `emplace`, `index`, `valueless_by_exception`, `swap`, `get`, `get_if`, `holds_alternative`, `visit` (with and without an explicit result type, over one or several variants), `monostate`, `bad_variant_access`, `variant_npos`, `variant_size`, `variant_alternative` (also as specializations of the `std` traits), the six comparisons and `<=>` when the alternatives have them, `std::hash` when they do. What differs: nothing is `constexpr` (the alternatives live in raw storage), the variant is never trivially copyable, and its size is the pointer word plus the places of the alternatives that may hold pointers plus the largest of the others (`variant<int, tracked_ptr<T>, weak_ptr<T>>` is 16 bytes). A variant of pointer-free alternatives has no reason to be one of these: `std::variant` is smaller and `constexpr`.

The variant has no word of its own. Where it may live is decided by its alternatives: with `tracked_ptr` or `weak_ptr` alternatives, where a `tracked_ptr` may (on a stack or inside a managed object).

## Rules

- The alternatives follow their own rules where the variant lives: an `sgcl::tracked_ptr` alternative in a variant on the unmanaged heap is the same mistake as an `sgcl::tracked_ptr` in a `std::vector` ([The rules](README.md#the-rules), 1).
- An alternative that may hold pointers is traced through the pointer map of the object holding the variant, like a member at the same offset would be; its own data words leave the map by elimination, its pointer words never do.
- A pointer alternative is destroyed when another is emplaced or assigned, or the variant is destroyed: its object is unreferenced from then on, and dies with the next cycle that finds it so.
- Thread safety is that of `std::variant`: threads share one with the program's own synchronization ([The rules](README.md#the-rules), 6).
- In a destructor, a pointer alternative is a `tracked_ptr` member: not to be read there ([The rules](README.md#the-rules), 5).

## Members

```cpp
variant();                                                  // the first alternative, value-initialized
variant(const variant&);
variant(variant&&) noexcept(...);
template<class U> variant(U&& u);                           // the alternative std::variant's overload resolution selects
template<class T, class... A> explicit variant(std::in_place_type_t<T>, A&&...);
template<class T, class U, class... A> explicit variant(std::in_place_type_t<T>, std::initializer_list<U>, A&&...);
template<size_t I, class... A> explicit variant(std::in_place_index_t<I>, A&&...);
template<size_t I, class U, class... A> explicit variant(std::in_place_index_t<I>, std::initializer_list<U>, A&&...);
~variant();

variant& operator=(const variant&);
variant& operator=(variant&&) noexcept(...);
template<class U> variant& operator=(U&& u);

template<class T, class... A> T& emplace(A&&...);
template<class T, class U, class... A> T& emplace(std::initializer_list<U>, A&&...);
template<size_t I, class... A> variant_alternative_t<I, variant>& emplace(A&&...);
template<size_t I, class U, class... A> variant_alternative_t<I, variant>& emplace(std::initializer_list<U>, A&&...);

size_t index() const noexcept;                              // variant_npos when valueless
bool valueless_by_exception() const noexcept;
void swap(variant&) noexcept(...);
```

The free functions, in `sgcl` (found by argument-dependent lookup on a `sgcl::variant`):

```cpp
template<class T, class... Ts> bool holds_alternative(const variant<Ts...>&) noexcept;
template<size_t I, class... Ts> variant_alternative_t<I, variant<Ts...>>& get(variant<Ts...>&);          // and const, &&, const &&
template<class T, class... Ts> T& get(variant<Ts...>&);                                                    // and const, &&, const &&
template<size_t I, class... Ts> std::add_pointer_t<variant_alternative_t<I, variant<Ts...>>> get_if(variant<Ts...>*) noexcept;   // and const
template<class T, class... Ts> std::add_pointer_t<T> get_if(variant<Ts...>*) noexcept;                    // and const
template<class F, class... Vs> decltype(auto) visit(F&& f, Vs&&... vs);
template<class R, class F, class... Vs> R visit(F&& f, Vs&&... vs);
template<class... Ts> void swap(variant<Ts...>&, variant<Ts...>&) noexcept(...);
bool operator==, !=, <, <=, >, >= (const variant<Ts...>&, const variant<Ts...>&);   // when every alternative has ==, or <
auto operator<=>(const variant<Ts...>&, const variant<Ts...>&);                     // when every alternative has <=>
template<class T> struct variant_size; template<size_t I, class T> struct variant_alternative;   // and _v, _t; std::variant_size, std::variant_alternative specialized
template<class... Ts> struct std::hash<sgcl::variant<Ts...>>;                       // when every alternative has a hash
```

`get` throws `bad_variant_access` (the one of `std`) on the wrong index, `visit` on a valueless variant. `visit` over several variants calls the visitor with one alternative of each; the visitor (a callable or a pointer to member, through `std::invoke`) returns one type for every combination of alternatives, as with `std::visit` (ill-formed otherwise: a constraint), or `visit<R>` converts each result to the `R` given. `get<T>`, `get_if<T>` and `holds_alternative<T>` are ill-formed for a type that is not exactly one alternative.

```cpp
struct Node { int value; };
using Value = sgcl::variant<int, sgcl::tracked_ptr<Node>, sgcl::string>;

Value v = sgcl::make_tracked<Node>(1);           // index 1: the pointer, in the word of its own
assert(sgcl::holds_alternative<sgcl::tracked_ptr<Node>>(v));
assert(sgcl::get<1>(v)->value == 1);
v = 5;                                           // index 0: the int; the pointer destroyed, the word null
v = "text";                                      // index 2: the string
auto described = sgcl::visit([](const auto& x) -> sgcl::string {
    if constexpr(std::is_same_v<std::remove_cvref_t<decltype(x)>, sgcl::string>) {
        return x;
    } else {
        return "not a string";
    }
}, v);
assert(described == "text");
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A tree of values: a leaf holds a number or a name, a branch holds its
// children. The children are tracked pointers, next to the data, in one
// variant per node: the collector follows them.
struct Node;
using Children = sgcl::vector<sgcl::tracked_ptr<Node>>;
struct Node {
    sgcl::variant<double, sgcl::string, Children> value;
};

double sum(const sgcl::tracked_ptr<Node>& node) {
    return sgcl::visit([](const auto& v) -> double {
        using T = std::remove_cvref_t<decltype(v)>;
        if constexpr(std::is_same_v<T, double>) {
            return v;
        } else if constexpr(std::is_same_v<T, Children>) {
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
    sgcl::tracked_ptr root = sgcl::make_tracked<Node>();
    Children children;
    children.push_back(sgcl::make_tracked<Node>(Node{1.5}));
    children.push_back(sgcl::make_tracked<Node>(Node{sgcl::string("name")}));
    children.push_back(sgcl::make_tracked<Node>(Node{2.5}));
    root->value = std::move(children);
    std::cout << sum(root) << "\n";              // 4
    root->value = 0.0;                           // the children unreferenced: collected
    sgcl::collector::force_collect(true);          // optional, for the demonstration only
    return 0;
}
```

The output:

```
4
```

## See also

- [any](any.md): the same for a value of any type; [tracked_ptr](tracked_ptr.md), [weak_ptr](weak_ptr.md): the pointer words
- README: [variant, any, function and expected](README.md#variant-any-function-and-expected), [Pointer maps](../../garbage_collector/overview.md#pointer-maps), [The rules](README.md#the-rules)
- `tests/core/variant.cpp`: every behaviour above, checked.
