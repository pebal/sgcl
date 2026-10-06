// The counterparts of benchmarks/async/coordination.cpp in Go. The
// singleflight group is golang.org/x/sync/singleflight's algorithm written
// out (a mutex around a map of the calls in flight, a WaitGroup per call,
// the first caller running the function). The retry is a loop of the same
// policy (exponential waits with full jitter from math/rand/v2, a timer
// beside the context), Go having none of its own. The rate
// limiter is the algorithm of golang.org/x/time/rate written out here (a
// mutex around a float of tokens and the time of the last update, the
// tokens advanced by the time passed, a reservation that may go into debt
// and a Wait that sleeps on a timer beside the context), since the x module
// is not on this machine and the committed programs use the standard
// library only. One case per run; prints one line, ns per operation.
package main

import (
	"context"
	"errors"
	"fmt"
	"math"
	"math/rand/v2"
	"os"
	"strconv"
	"sync"
	"syscall"
	"time"
)

func cpuSeconds() float64 {
	var ru syscall.Rusage
	syscall.Getrusage(syscall.RUSAGE_SELF, &ru)
	return float64(ru.Utime.Sec) + float64(ru.Utime.Usec)*1e-6 + float64(ru.Stime.Sec) + float64(ru.Stime.Usec)*1e-6
}

func report(what string, wall float64, ops int64) {
	fmt.Printf("coordination %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs\n", what, wall*1e9/float64(ops), float64(ops)/wall, wall, cpuSeconds())
}

// The token bucket of x/time/rate
type limiter struct {
	mu     sync.Mutex
	limit  float64
	burst  int
	tokens float64
	last   time.Time
}

func newLimiter(limit float64, burst int) *limiter {
	return &limiter{limit: limit, burst: burst, tokens: float64(burst)}
}

func (l *limiter) advance(t time.Time) (time.Time, float64) {
	last := l.last
	if t.Before(last) {
		last = t
	}
	elapsed := t.Sub(last)
	tokens := l.tokens + elapsed.Seconds()*l.limit
	if b := float64(l.burst); tokens > b {
		tokens = b
	}
	return t, tokens
}

func (l *limiter) reserveN(t time.Time, n int, maxWait time.Duration) (bool, time.Time) {
	l.mu.Lock()
	defer l.mu.Unlock()
	t, tokens := l.advance(t)
	tokens -= float64(n)
	var wait time.Duration
	if tokens < 0 {
		wait = time.Duration(-tokens / l.limit * 1e9)
	}
	ok := n <= l.burst && wait <= maxWait
	if ok {
		l.last = t
		l.tokens = tokens
		return true, t.Add(wait)
	}
	return false, time.Time{}
}

func (l *limiter) allow() bool {
	ok, _ := l.reserveN(time.Now(), 1, 0)
	return ok
}

func (l *limiter) wait(ctx context.Context) error {
	select {
	case <-ctx.Done():
		return ctx.Err()
	default:
	}
	now := time.Now()
	ok, act := l.reserveN(now, 1, time.Duration(1<<62))
	if !ok {
		return fmt.Errorf("rate: wait exceeds burst")
	}
	d := act.Sub(now)
	if d <= 0 {
		return nil
	}
	t := time.NewTimer(d)
	defer t.Stop()
	select {
	case <-t.C:
		return nil
	case <-ctx.Done():
		return ctx.Err()
	}
}

// The group of x/sync/singleflight
type call struct {
	wg  sync.WaitGroup
	val int64
}

type group struct {
	mu sync.Mutex
	m  map[string]*call
}

func (g *group) do(key string, fn func() int64) int64 {
	g.mu.Lock()
	if g.m == nil {
		g.m = make(map[string]*call)
	}
	if c, ok := g.m[key]; ok {
		g.mu.Unlock()
		c.wg.Wait()
		return c.val
	}
	c := new(call)
	c.wg.Add(1)
	g.m[key] = c
	g.mu.Unlock()
	c.val = fn()
	g.mu.Lock()
	if g.m[key] == c {
		delete(g.m, key)
	}
	g.mu.Unlock()
	c.wg.Done()
	return c.val
}

// The work of a shared call: 2 us of the processor
func spin2us() int64 {
	until := time.Now().Add(2 * time.Microsecond)
	for time.Now().Before(until) {
	}
	return 1
}

// A retry with exponential backoff and full jitter
type policy struct {
	attempts   int
	maxElapsed time.Duration
	initial    time.Duration
	maxDelay   time.Duration
	multiplier float64
}

func defaultPolicy() policy {
	return policy{attempts: 5, initial: 100 * time.Millisecond, maxDelay: 10 * time.Second, multiplier: 2}
}

func retry[T any](ctx context.Context, p policy, f func() (T, error)) (T, error) {
	var start time.Time
	if p.maxElapsed > 0 {
		start = time.Now()
	}
	for n := 1; ; n++ {
		v, err := f()
		if err == nil {
			return v, nil
		}
		if p.attempts != 0 && n >= p.attempts {
			return v, err
		}
		d := float64(p.initial) * math.Pow(p.multiplier, float64(n-1))
		if d > float64(p.maxDelay) {
			d = float64(p.maxDelay)
		}
		wait := time.Duration(rand.Float64() * d)
		if p.maxElapsed > 0 && time.Since(start)+wait > p.maxElapsed {
			return v, err
		}
		if wait > 0 {
			t := time.NewTimer(wait)
			select {
			case <-t.C:
			case <-ctx.Done():
				t.Stop()
				return v, err
			}
		}
	}
}

var errBusy = errors.New("busy")

func main() {
	what := "allow"
	if len(os.Args) > 1 {
		what = os.Args[1]
	}
	var n int64
	if len(os.Args) > 2 {
		n, _ = strconv.ParseInt(os.Args[2], 10, 64)
	}
	var sum int64
	ctx := context.Background()
	switch what {
	case "allow": // tokens always there: 1e9 a second, a burst of 1e9
		if n == 0 {
			n = 20000000
		}
		l := newLimiter(1e9, 1000000000)
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if l.allow() {
				sum++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "deny": // an empty bucket: 1 a second, burst 1, taken
		if n == 0 {
			n = 20000000
		}
		l := newLimiter(1, 1)
		l.allow()
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if l.allow() {
				sum++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "allowpar": // 8 goroutines on one bucket, per call
		if n == 0 {
			n = 4000000
		}
		l := newLimiter(1e9, 1000000000)
		var wg sync.WaitGroup
		t0 := time.Now()
		for g := 0; g < 8; g++ {
			wg.Add(1)
			go func() {
				defer wg.Done()
				for i := int64(0); i < n; i++ {
					l.allow()
				}
			}()
		}
		wg.Wait()
		report(what, time.Since(t0).Seconds(), 8*n)
	case "waitnow": // Wait with the tokens there
		if n == 0 {
			n = 5000000
		}
		l := newLimiter(1e9, 1000000000)
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if l.wait(ctx) == nil {
				sum++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "paced": // Wait at 10000 a second, burst 1: per wait (100 us is exact)
		if n == 0 {
			n = 10000
		}
		l := newLimiter(10000, 1)
		l.allow()
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			if l.wait(ctx) == nil {
				sum++
			}
		}
		report(what, time.Since(t0).Seconds(), n)
	case "sfsolo", "sftask": // one caller, the function returns at once: a call made and ended
		if n == 0 {
			n = 5000000
		}
		var g group
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			v := i
			sum += g.do("key", func() int64 { return v })
		}
		report(what, time.Since(t0).Seconds(), n)
	case "sfpar": // 8 goroutines on 16 keys, per call
		if n == 0 {
			n = 1000000
		}
		var g group
		var wg sync.WaitGroup
		t0 := time.Now()
		for t := 0; t < 8; t++ {
			wg.Add(1)
			go func(t int) {
				defer wg.Done()
				keys := make([]string, 16)
				for k := range keys {
					keys[k] = strconv.Itoa(k)
				}
				for i := int64(0); i < n; i++ {
					v := i
					g.do(keys[(i+int64(t))&15], func() int64 { return v })
				}
			}(t)
		}
		wg.Wait()
		report(what, time.Since(t0).Seconds(), 8*n)
	case "sfshared": // 8 goroutines on one key, the function 2 us of work: per caller's call
		if n == 0 {
			n = 500000
		}
		var g group
		var wg sync.WaitGroup
		t0 := time.Now()
		for t := 0; t < 8; t++ {
			wg.Add(1)
			go func() {
				defer wg.Done()
				for i := int64(0); i < n; i++ {
					g.do("key", spin2us)
				}
			}()
		}
		wg.Wait()
		report(what, time.Since(t0).Seconds(), 8*n)
	case "retryok", "retrytask": // a value at once: per retry
		if n == 0 {
			n = 10000000
		}
		p := defaultPolicy()
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			v := i
			r, _ := retry(ctx, p, func() (int64, error) { return v, nil })
			sum += r
		}
		report(what, time.Since(t0).Seconds(), n)
	case "retryfail": // three failures, then a value, waits of zero: per retry
		if n == 0 {
			n = 5000000
		}
		p := defaultPolicy()
		p.initial = 0
		t0 := time.Now()
		for i := int64(0); i < n; i++ {
			left := 3
			v := i
			r, _ := retry(ctx, p, func() (int64, error) {
				if left > 0 {
					left--
					return 0, errBusy
				}
				return v, nil
			})
			sum += r
		}
		report(what, time.Since(t0).Seconds(), n)
	default:
		fmt.Fprintln(os.Stderr, "unknown case", what)
		os.Exit(2)
	}
	if sum < 0 {
		fmt.Println(sum)
	}
}
