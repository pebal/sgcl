package main

// Go's side of the gzip stream test (../server.cpp is the module's):
//
//	gzbench server [address=:18201 [certificate.pem key.pem]]
//	    for every byte a client sends, the same 256 KB of text compressed by
//	    gzip.NewWriter (the default level) on the net.Conn or tls.Conn itself,
//	    one member a response, the writer reset for the next
//	gzbench load -addr host:port [-ca ca.pem] -c C -d D
//	    C connections (TLS 1.3 when -ca is given, made before the clock), one
//	    response in flight each: a byte sent, the member read to its end
//	    through gzip.Reader; one line:
//	    load: N req/s, errors E, p50 X, p99 Y
import (
	"bufio"
	"compress/gzip"
	"crypto/tls"
	"crypto/x509"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"sort"
	"sync"
	"sync/atomic"
	"time"
)

// 256 KB of words from a small vocabulary, the module's generator
func text() []byte {
	words := []string{"the", "quick", "brown", "fox", "jumps", "over", "a", "lazy", "dog", "and", "runs", "far",
		"away", "from", "home", "while", "birds", "sing", "in", "trees", "under", "blue", "sky", "today"}
	s := make([]byte, 0, 256<<10+16)
	x := uint32(2463534242)
	for len(s) < 256<<10 {
		x ^= x << 13
		x ^= x >> 17
		x ^= x << 5
		s = append(s, words[x%24]...)
		if (x>>8)%11 == 0 {
			s = append(s, ".\n"...)
		} else {
			s = append(s, ' ')
		}
	}
	return s[:256<<10]
}

func serve(c net.Conn, data []byte) {
	defer c.Close()
	gz := gzip.NewWriter(c)
	ask := make([]byte, 64)
	for {
		n, err := c.Read(ask)
		if err != nil || n == 0 {
			return
		}
		for k := 0; k < n; k++ {
			gz.Reset(c)
			if _, err := gz.Write(data); err != nil {
				return
			}
			if err := gz.Close(); err != nil {
				return
			}
		}
	}
}

func server(args []string) {
	addr := ":18201"
	if len(args) > 0 {
		addr = args[0]
	}
	var l net.Listener
	var err error
	if len(args) > 2 {
		cert, e := tls.LoadX509KeyPair(args[1], args[2])
		if e != nil {
			fmt.Println("cert:", e)
			os.Exit(1)
		}
		l, err = tls.Listen("tcp", addr, &tls.Config{Certificates: []tls.Certificate{cert}, MinVersion: tls.VersionTLS13})
	} else {
		l, err = net.Listen("tcp", addr)
	}
	if err != nil {
		fmt.Println("listen:", err)
		os.Exit(1)
	}
	data := text()
	for {
		c, err := l.Accept()
		if err != nil {
			return
		}
		go serve(c, data)
	}
}

func load(args []string) {
	fs := flag.NewFlagSet("load", flag.ExitOnError)
	addr := fs.String("addr", "127.0.0.1:18200", "server")
	ca := fs.String("ca", "", "TLS: the CA of the server's certificate (PEM)")
	conns := fs.Int("c", 16, "connections")
	dur := fs.Duration("d", 2*time.Second, "duration")
	fs.Parse(args)
	var cfg *tls.Config
	if *ca != "" {
		pem, err := os.ReadFile(*ca)
		if err != nil {
			fmt.Println("ca:", err)
			os.Exit(1)
		}
		pool := x509.NewCertPool()
		pool.AppendCertsFromPEM(pem)
		cfg = &tls.Config{RootCAs: pool, ServerName: "localhost", MinVersion: tls.VersionTLS13}
	}
	cs := make([]net.Conn, *conns)
	for i := range cs {
		c, err := net.Dial("tcp", *addr)
		if err != nil {
			fmt.Println("dial:", err)
			os.Exit(1)
		}
		if cfg != nil {
			tc := tls.Client(c, cfg)
			if err := tc.Handshake(); err != nil {
				fmt.Println("handshake:", err)
				os.Exit(1)
			}
			c = tc
		}
		cs[i] = c
	}
	var done, errs atomic.Int64
	var mu sync.Mutex
	var lat []time.Duration
	until := time.Now().Add(*dur)
	start := time.Now()
	var wg sync.WaitGroup
	for _, c := range cs {
		wg.Add(1)
		go func(c net.Conn) {
			defer wg.Done()
			br := bufio.NewReaderSize(c, 64<<10)
			zr := new(gzip.Reader)
			mine := make([]time.Duration, 0, 4096)
			one := []byte{'g'}
			for time.Now().Before(until) {
				t0 := time.Now()
				if _, err := c.Write(one); err != nil {
					errs.Add(1)
					return
				}
				if err := zr.Reset(br); err != nil {
					errs.Add(1)
					return
				}
				zr.Multistream(false)
				if _, err := io.Copy(io.Discard, zr); err != nil {
					errs.Add(1)
					return
				}
				mine = append(mine, time.Since(t0))
				done.Add(1)
			}
			mu.Lock()
			lat = append(lat, mine...)
			mu.Unlock()
		}(c)
	}
	wg.Wait()
	wall := time.Since(start)
	for _, c := range cs {
		c.Close()
	}
	sort.Slice(lat, func(i, j int) bool { return lat[i] < lat[j] })
	p := func(q float64) time.Duration {
		if len(lat) == 0 {
			return 0
		}
		return lat[int(q*float64(len(lat)-1))]
	}
	fmt.Printf("load: %d req/s, errors %d, p50 %v, p99 %v\n", int(float64(done.Load())/wall.Seconds()), errs.Load(), p(0.50), p(0.99))
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("gzbench server|load ...")
		os.Exit(2)
	}
	switch os.Args[1] {
	case "server":
		server(os.Args[2:])
	case "load":
		load(os.Args[2:])
	default:
		fmt.Println("gzbench server|load ...")
		os.Exit(2)
	}
}
