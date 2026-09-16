// The Go counterpart of benchmarks/concurrent.cpp: the same three
// lock-free structures on Go's collector, written as SGCL's are, with
// atomic.Pointer for every link. Go's library has none of them (a channel
// is a lock and a buffer), so they are the algorithms themselves: the
// Michael–Scott queue, the Treiber stack, and the skip list of Herlihy
// and Shavit with marker nodes for the deletions; the collector takes
// care of ABA and of the memory. An element is an item of one int64.
//   concurrent <queue|stack> [threads=4] [mode=mixed] [n=200000]
//   concurrent <map|umap|set> [threads=4] [keys=200000] [n=200000]
//   concurrent cow [threads=16] [n=2000000]
//   concurrent chan [threads=4] [capacity=64] [n=200000]
// umap is sync.Map, the concurrent map of Go's library (a hash map with
// reads from a snapshot and writes under a lock, its keys boxed in any);
// set the skip list holding keys alone. cow is Go's idiom for a value
// read by many and replaced whole: an atomic.Pointer to an array of 64
// int64, the readers loading and summing it n times each, one writer
// copying, changing an element and swapping it in as fast as it can.
// chan is Go's channel of *item with the given capacity (0: unbuffered)
// between threads / 2 producers of n items each and threads / 2
// consumers.
package main

import (
	"fmt"
	"math/bits"
	"math/rand/v2"
	"os"
	"runtime"
	"strconv"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

type item struct {
	value int64
}

// Michael–Scott: the head addresses a dummy whose item was taken; the
// head and the tail a cache line apart.
type qnode struct {
	next atomic.Pointer[qnode]
	item *item
}

type queue struct {
	head atomic.Pointer[qnode]
	_    [120]byte
	tail atomic.Pointer[qnode]
}

func newQueue() *queue {
	d := &qnode{}
	q := &queue{}
	q.head.Store(d)
	q.tail.Store(d)
	return q
}

func (q *queue) push(v int64) {
	n := &qnode{item: &item{value: v}}
	for {
		t := q.tail.Load()
		next := t.next.Load()
		if next != nil {
			q.tail.CompareAndSwap(t, next)
			continue
		}
		if t.next.CompareAndSwap(nil, n) {
			q.tail.CompareAndSwap(t, n)
			return
		}
	}
}

func (q *queue) pop() int64 {
	for {
		h := q.head.Load()
		next := h.next.Load()
		if next == nil {
			return -1
		}
		t := q.tail.Load()
		if h == t {
			q.tail.CompareAndSwap(t, next)
			continue
		}
		if q.head.CompareAndSwap(h, next) {
			v := next.item.value
			next.item = nil
			return v
		}
	}
}

// Treiber, with the backoff of sgcl/detail/backoff.h: a wait that doubles
// after every lost exchange, up to backoffMax pauses: an isb on arm64
// (isb_arm64.s, the pause of the C++ variants), the loop alone elsewhere
const backoffMax = 4096

func backoff(pauses *int) {
	for i := 0; i < *pauses; i++ {
		isb()
	}
	if *pauses < backoffMax {
		*pauses *= 2
	}
}

type snode struct {
	next *snode
	item *item
}

type stack struct {
	head atomic.Pointer[snode]
}

func (s *stack) push(v int64) {
	n := &snode{item: &item{value: v}}
	pauses := 1
	for {
		h := s.head.Load()
		n.next = h
		if s.head.CompareAndSwap(h, n) {
			return
		}
		backoff(&pauses)
	}
}

func (s *stack) pop() int64 {
	pauses := 1
	for {
		h := s.head.Load()
		if h == nil {
			return -1
		}
		if s.head.CompareAndSwap(h, h.next) {
			v := h.item.value
			h.item = nil
			return v
		}
		backoff(&pauses)
	}
}

// The skip list of sgcl/concurrent_map.h: the bottom link in the node,
// the upper links in a slice made only for a node of height above one
// (a quarter of them), a marker a node of its own linked after the node
// it marks at one level.
const maxHeight = 32

type mnode struct {
	next   atomic.Pointer[mnode]
	up     []atomic.Pointer[mnode]
	key    int64
	val    *item
	height uint8
	marker bool
}

func (n *mnode) link(level int) *atomic.Pointer[mnode] {
	if level == 0 {
		return &n.next
	}
	return &n.up[level-1]
}

func newNode(key int64, val *item, height int) *mnode {
	n := &mnode{key: key, val: val, height: uint8(height)}
	if height > 1 {
		n.up = make([]atomic.Pointer[mnode], height-1)
	}
	return n
}

func newMarker(succ *mnode) *mnode {
	m := &mnode{height: 1, marker: true}
	m.next.Store(succ)
	return m
}

type skiplist struct {
	head *mnode
	top  atomic.Uint32
}

func newSkipList() *skiplist {
	s := &skiplist{head: newNode(0, nil, maxHeight)}
	s.top.Store(1)
	return s
}

func (s *skiplist) randomHeight() int {
	h := 1 + bits.TrailingZeros64(rand.Uint64()|(1<<62))/2
	top := s.top.Load()
	if uint32(h) > top+1 {
		h = int(top + 1)
	}
	for uint32(h) > top && !s.top.CompareAndSwap(top, uint32(h)) {
		top = s.top.Load()
	}
	return h
}

// the wait-free search: the first node of the bottom list whose key is
// not less than key, stepping over erased nodes
func (s *skiplist) search(key int64) *mnode {
	top := int(s.top.Load())
	pred := s.head
	var curr *mnode
	for level := top - 1; level >= 0; level-- {
		curr = pred.link(level).Load()
		for {
			if curr != nil && curr.marker {
				curr = curr.next.Load()
				continue
			}
			if curr == nil {
				break
			}
			succ := curr.link(level).Load()
			if succ != nil && succ.marker {
				curr = succ.next.Load()
				continue
			}
			if !(curr.key < key) {
				break
			}
			pred = curr
			curr = succ
		}
	}
	return curr
}

// the search that unlinks erased nodes on the way; preds and succs of
// every level in use
func (s *skiplist) find(key int64, preds, succs *[maxHeight]*mnode) (bool, int) {
retry:
	top := int(s.top.Load())
	pred := s.head
	var curr *mnode
	for level := top - 1; level >= 0; level-- {
		curr = pred.link(level).Load()
		for {
			if curr != nil && curr.marker {
				goto retry
			}
			if curr == nil {
				break
			}
			succ := curr.link(level).Load()
			if succ != nil && succ.marker {
				after := succ.next.Load()
				if !pred.link(level).CompareAndSwap(curr, after) {
					goto retry
				}
				curr = after
				continue
			}
			if !(curr.key < key) {
				break
			}
			pred = curr
			curr = succ
		}
		preds[level] = pred
		succs[level] = curr
	}
	return curr != nil && !(key < curr.key), top
}

func (s *skiplist) insert(key int64, val *item) bool {
	h := s.randomHeight()
	node := newNode(key, val, h)
	var preds, succs [maxHeight]*mnode
	for {
		found, _ := s.find(key, &preds, &succs)
		if found {
			return false
		}
		for i := 0; i < h; i++ {
			node.link(i).Store(succs[i])
		}
		if preds[0].link(0).CompareAndSwap(succs[0], node) {
			break
		}
	}
	for i := 1; i < h; i++ {
		for {
			if preds[i].link(i).CompareAndSwap(succs[i], node) {
				break
			}
			if found, _ := s.find(key, &preds, &succs); !found || succs[0] != node {
				return true
			}
			old := node.link(i).Load()
			if old != nil && old.marker {
				return true
			}
			if old != succs[i] && !node.link(i).CompareAndSwap(old, succs[i]) {
				return true
			}
		}
	}
	return true
}

func (s *skiplist) get(key int64) *item {
	n := s.search(key)
	if n != nil && !(key < n.key) {
		return n.val
	}
	return nil
}

func (s *skiplist) erase(key int64) bool {
	var preds, succs [maxHeight]*mnode
	found, _ := s.find(key, &preds, &succs)
	if !found {
		return false
	}
	node := succs[0]
	for level := int(node.height) - 1; level >= 1; level-- {
		succ := node.link(level).Load()
		for !(succ != nil && succ.marker) {
			if node.link(level).CompareAndSwap(succ, newMarker(succ)) {
				break
			}
			succ = node.link(level).Load()
		}
	}
	succ := node.next.Load()
	for {
		if succ != nil && succ.marker {
			return false
		}
		if node.next.CompareAndSwap(succ, newMarker(succ)) {
			s.find(key, &preds, &succs)
			return true
		}
		succ = node.next.Load()
	}
}

type values [64]int64

func runCow(threads int, n int64) {
	var v atomic.Pointer[values]
	v.Store(&values{})
	var stop atomic.Bool
	var writes int64
	var writeTime float64
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads-1; t++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			var sum int64
			for i := int64(0); i < n; i++ {
				s := v.Load()
				for _, x := range s {
					sum += x
				}
			}
			if sum == -1 {
				fmt.Print("?")
			}
		}()
	}
	var ww sync.WaitGroup
	ww.Add(1)
	go func() {
		defer ww.Done()
		w0 := time.Now()
		var i int64
		for !stop.Load() {
			for {
				old := v.Load()
				next := *old
				next[i%64] = (next[i%64] + 1) % 100
				if v.CompareAndSwap(old, &next) {
					break
				}
			}
			i++
		}
		writes = i
		writeTime = time.Since(w0).Seconds()
	}()
	wg.Wait()
	stop.Store(true)
	ww.Wait()
	wall := time.Since(t0).Seconds()
	reads := float64(n) * float64(threads-1)
	perWrite := 0.0
	if writes > 0 {
		perWrite = writeTime * 1e9 / float64(writes)
	}
	fmt.Printf("cow threads=%d ns/read=%.1f ns/write=%.1f writes=%d reads/s=%.0f wall=%.2fs cpu=%.2fs\n", threads, wall*1e9/reads, perWrite, writes, reads/wall, wall, cpuSeconds())
}

func runChan(threads int, capacity int, n int64) {
	ch := make(chan *item, capacity)
	producers, consumers := max(1, threads/2), max(1, threads/2)
	var pw, cw sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < producers; t++ {
		pw.Add(1)
		go func() {
			defer pw.Done()
			for i := int64(0); i < n; i++ {
				ch <- &item{value: i}
			}
		}()
	}
	for t := 0; t < consumers; t++ {
		cw.Add(1)
		go func() {
			defer cw.Done()
			var sum int64
			for it := range ch {
				sum += it.value
			}
			if sum == -1 {
				fmt.Print("?")
			}
		}()
	}
	pw.Wait()
	close(ch)
	cw.Wait()
	wall := time.Since(t0).Seconds()
	ops := float64(n) * float64(producers)
	fmt.Printf("chan threads=%d capacity=%d ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", threads, capacity, wall*1e9/ops, ops/wall, wall, cpuSeconds())
}

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

type container interface {
	push(v int64)
	pop() int64
}

func runContainer(what string, c container, threads int, mode string, n int64) {
	pairs := mode == "pairs"
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func(t int) {
			defer wg.Done()
			var sum int64
			if !pairs {
				for i := int64(0); i < n; i++ {
					c.push(i)
					sum += c.pop()
				}
			} else if t%2 == 0 {
				for i := int64(0); i < n; i++ {
					c.push(i)
				}
			} else {
				for i := int64(0); i < n; {
					v := c.pop()
					if v >= 0 {
						sum += v
						i++
					} else {
						runtime.Gosched()
					}
				}
			}
			if sum == -1 {
				fmt.Print("?")
			}
		}(t)
	}
	wg.Wait()
	wall := time.Since(t0).Seconds()
	ops := float64(n) * float64(threads)
	if !pairs {
		ops *= 2
	}
	fmt.Printf("%s threads=%d mode=%s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, threads, mode, wall*1e9/ops, ops/wall, wall, cpuSeconds())
}

func phase(threads int, body func(t int)) float64 {
	var wg sync.WaitGroup
	t0 := time.Now()
	for t := 0; t < threads; t++ {
		wg.Add(1)
		go func(t int) {
			defer wg.Done()
			body(t)
		}(t)
	}
	wg.Wait()
	return time.Since(t0).Seconds()
}

type keyed interface {
	insert(k int64) bool
	find(k int64) int64
	erase(k int64) bool
}

type skipMap struct{ m *skiplist }

func (s skipMap) insert(k int64) bool { return s.m.insert(k, &item{value: k}) }
func (s skipMap) find(k int64) int64 {
	if it := s.m.get(k); it != nil {
		return it.value
	}
	return -1
}
func (s skipMap) erase(k int64) bool { return s.m.erase(k) }

type skipSet struct{ m *skiplist }

func (s skipSet) insert(k int64) bool { return s.m.insert(k, nil) }
func (s skipSet) find(k int64) int64 {
	if n := s.m.search(k); n != nil && !(k < n.key) {
		return k
	}
	return -1
}
func (s skipSet) erase(k int64) bool { return s.m.erase(k) }

type syncMap struct{ m sync.Map }

func (s *syncMap) insert(k int64) bool {
	_, loaded := s.m.LoadOrStore(k, &item{value: k})
	return !loaded
}
func (s *syncMap) find(k int64) int64 {
	if v, ok := s.m.Load(k); ok {
		return v.(*item).value
	}
	return -1
}
func (s *syncMap) erase(k int64) bool {
	_, loaded := s.m.LoadAndDelete(k)
	return loaded
}

func runMap(what string, m keyed, threads int, keys, n int64) {
	insert := phase(threads, func(t int) {
		for k := int64(t); k < keys; k += int64(threads) {
			m.insert(k)
		}
	})
	find := phase(threads, func(t int) {
		rng := rand.New(rand.NewPCG(1234, uint64(t)))
		var sum int64
		for i := int64(0); i < n; i++ {
			sum += m.find(rng.Int64N(keys))
		}
		if sum == -1 {
			fmt.Print("?")
		}
	})
	mixed := phase(threads, func(t int) {
		rng := rand.New(rand.NewPCG(4321, uint64(t)))
		var sum int64
		for i := int64(0); i < n; i++ {
			r := rng.Uint64()
			k := int64((r >> 8) % uint64(2*keys))
			op := r & 0xFF
			if op < 205 {
				sum += m.find(k)
			} else if op < 230 {
				if m.insert(k) {
					sum++
				}
			} else if m.erase(k) {
				sum++
			}
		}
		if sum == -1 {
			fmt.Print("?")
		}
	})
	ops := float64(n) * float64(threads)
	fmt.Printf("%s threads=%d keys=%d insert=%.1f find=%.1f mixed=%.1f wall=%.2fs cpu=%.2fs\n", what, threads, keys, insert*1e9/float64(keys), find*1e9/ops, mixed*1e9/ops, insert+find+mixed, cpuSeconds())
}

func main() {
	what := "queue"
	threads := 4
	if len(os.Args) > 1 {
		what = os.Args[1]
	}
	if len(os.Args) > 2 {
		threads, _ = strconv.Atoi(os.Args[2])
	}
	runtime.GOMAXPROCS(runtime.NumCPU())
	if what == "chan" {
		capacity, n := 64, int64(200000)
		if len(os.Args) > 3 {
			capacity, _ = strconv.Atoi(os.Args[3])
		}
		if len(os.Args) > 4 {
			n, _ = strconv.ParseInt(os.Args[4], 10, 64)
		}
		runChan(threads, capacity, n)
		return
	}
	if what == "cow" {
		n := int64(2000000)
		if len(os.Args) > 3 {
			n, _ = strconv.ParseInt(os.Args[3], 10, 64)
		}
		if len(os.Args) <= 2 {
			threads = 16
		}
		runCow(threads, n)
		return
	}
	if what == "map" || what == "umap" || what == "set" {
		keys, n := int64(200000), int64(200000)
		if len(os.Args) > 3 {
			keys, _ = strconv.ParseInt(os.Args[3], 10, 64)
		}
		if len(os.Args) > 4 {
			n, _ = strconv.ParseInt(os.Args[4], 10, 64)
		}
		var m keyed
		switch what {
		case "map":
			m = skipMap{newSkipList()}
		case "umap":
			m = &syncMap{}
		default:
			m = skipSet{newSkipList()}
		}
		runMap(what, m, threads, keys, n)
		return
	}
	mode, n := "mixed", int64(200000)
	if len(os.Args) > 3 {
		mode = os.Args[3]
	}
	if len(os.Args) > 4 {
		n, _ = strconv.ParseInt(os.Args[4], 10, 64)
	}
	if what == "stack" {
		runContainer(what, &stack{}, threads, mode, n)
	} else {
		runContainer(what, newQueue(), threads, mode, n)
	}
}
