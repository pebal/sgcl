// JSON-RPC in Go's standard library: net/rpc/jsonrpc (JSON-RPC 1.0 over a
// stream of JSON values), client and server, the cases of
// benchmarks/net/jsonrpc.cpp. Prints one line, ns per operation.
//
//	serve                   the server on 127.0.0.1: prints "port N", serves until killed
//	jsonrpc_call ADDR [n]   Arith.Add of two integers, one call at a time: per call
//	jsonrpc_parallel ADDR [n]   the same from 64 goroutines over one connection: per call
//	jsonrpc_handle [n]      a request decoded and its response encoded by encoding/json (no network): per request
package main

import (
	"encoding/json"
	"fmt"
	"net"
	"net/rpc"
	"net/rpc/jsonrpc"
	"os"
	"strconv"
	"sync"
	"time"
)

type Args struct{ A, B int }

type Arith int

func (t *Arith) Add(args *Args, reply *int) error {
	*reply = args.A + args.B
	return nil
}

func report(what string, d time.Duration, n int) {
	fmt.Printf("jsonrpc %s ns/op=%.1f ops/s=%.0f wall=%.2fs\n", what, float64(d.Nanoseconds())/float64(n), float64(n)/d.Seconds(), d.Seconds())
}

func count(i, def int) int {
	if len(os.Args) > i {
		if n, err := strconv.Atoi(os.Args[i]); err == nil {
			return n
		}
	}
	return def
}

func main() {
	what := os.Args[1]
	switch what {
	case "serve":
		rpc.Register(new(Arith))
		l, err := net.Listen("tcp", "127.0.0.1:0")
		if err != nil {
			panic(err)
		}
		fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
		os.Stdout.Sync()
		for {
			c, err := l.Accept()
			if err != nil {
				return
			}
			go jsonrpc.ServeConn(c)
		}
	case "jsonrpc_handle":
		n := count(2, 300000)
		text := []byte(`{"jsonrpc":"2.0","method":"add","params":{"a":20,"b":22},"id":1}`)
		total := 0
		t0 := time.Now()
		for i := 0; i < n; i++ {
			var req struct {
				Jsonrpc string          `json:"jsonrpc"`
				Method  string          `json:"method"`
				Params  json.RawMessage `json:"params"`
				ID      json.RawMessage `json:"id"`
			}
			if err := json.Unmarshal(text, &req); err != nil {
				panic(err)
			}
			var p struct{ A, B int64 }
			json.Unmarshal(req.Params, &p)
			out, _ := json.Marshal(struct {
				Jsonrpc string          `json:"jsonrpc"`
				ID      json.RawMessage `json:"id"`
				Result  int64           `json:"result"`
			}{"2.0", req.ID, p.A + p.B})
			total += len(out)
		}
		report(what, time.Since(t0), n)
		_ = total
	case "jsonrpc_call", "jsonrpc_parallel":
		c, err := jsonrpc.Dial("tcp", os.Args[2])
		if err != nil {
			panic(err)
		}
		if what == "jsonrpc_call" {
			n := count(3, 50000)
			t0 := time.Now()
			for i := 0; i < n; i++ {
				var r int
				if err := c.Call("Arith.Add", &Args{20, 22}, &r); err != nil || r != 42 {
					panic(err)
				}
			}
			report(what, time.Since(t0), n)
		} else {
			n := count(3, 640000)
			var wg sync.WaitGroup
			t0 := time.Now()
			for k := 0; k < 64; k++ {
				wg.Add(1)
				go func() {
					defer wg.Done()
					for i := 0; i < n/64; i++ {
						var r int
						if err := c.Call("Arith.Add", &Args{20, 22}, &r); err != nil || r != 42 {
							panic(err)
						}
					}
				}()
			}
			wg.Wait()
			report(what, time.Since(t0), n)
		}
	}
}
