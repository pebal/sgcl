# SGCL - Concurrent Garbage Collector for C++

## Classes
```
gc::collected
gc::object

gc::ptr<T>
gc::weak_ptr<T>
gc::atomic_ptr<T>
gc::atomic_weak_ptr<T>
```

## Example usage
```
#include "sgcl.h"

struct node : public gc::collected {
  gc::ptr<node> l;
  gc::ptr<node> r;
};

gc::ptr<node> tree(int c) {
  auto n = gc::make<node>();
  if (c) {
    n->l = tree(c - 1);
    n->r = tree(c - 1);
  }
  return n;
}

int main() {
  tree(10);    
  return 0;
}
```
```
#include "sgcl.h"

class foo : public gc::collected {
};
class bar : public gc::collected {
};
struct baz : public foo, public bar {
};

int main() {
  auto a = gc::make<baz>();
  gc::ptr<foo> b = a;
  auto c = a.clone();
  auto d = gc::dynamic_pointer_cast<bar>(c);
  return 0;
}
```
```
#include "sgcl.h"

class foo : public gc::collected {
};

int main() {
  gc::atomic_ptr<foo> a = gc::make<foo>();
  gc::atomic_ptr<foo> b = a.load(std::memory_order_relaxed);
  auto c = b.exchange(gc::make<foo>()); 
  return 0;
}
```
