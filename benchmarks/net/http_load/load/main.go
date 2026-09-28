package main

// A raw HTTP/1.1 keep-alive load generator: C goroutines, one persistent
// connection each, one request in flight per connection, D seconds. With
// -ca, https: each connection a crypto/tls one (TLS 1.3, ALPN http/1.1,
// the server's certificate checked against the CA for "localhost"), its
// handshake made before the clock starts. -method, -path and -body N (a
// body of N bytes with its Content-Length) shape the request. -proto h2:
// HTTP/2 by net/http's Transport instead (h2 by ALPN with -ca, else h2c by
// prior knowledge): C goroutines, one request in flight each, as streams
// of the Transport's connections (one while the server's
// MAX_CONCURRENT_STREAMS allows), a first request made before the clock.
import (
	"bufio"
	"bytes"
	"crypto/tls"
	"crypto/x509"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"sort"
	"strconv"
	"sync"
	"sync/atomic"
	"time"
)

func main() {
	addr := flag.String("addr", "127.0.0.1:18080", "server")
	conns := flag.Int("c", 64, "connections")
	dur := flag.Duration("d", 10*time.Second, "duration")
	ca := flag.String("ca", "", "https: the CA of the server's certificate (PEM)")
	method := flag.String("method", "GET", "the request's method")
	path := flag.String("path", "/", "the request's path and query")
	bodyN := flag.Int("body", 0, "the request's body, bytes")
	proto := flag.String("proto", "h1", "h1 (raw HTTP/1.1) or h2 (net/http's Transport, h2 or h2c)")
	flag.Parse()
	body := bytes.Repeat([]byte{'b'}, *bodyN)
	if *proto == "h2" {
		loadH2(*addr, *conns, *dur, *ca, *method, *path, body)
		return
	}
	var cfg *tls.Config
	if *ca != "" {
		pem, err := os.ReadFile(*ca)
		if err != nil {
			fmt.Println("ca:", err)
			os.Exit(1)
		}
		roots := x509.NewCertPool()
		roots.AppendCertsFromPEM(pem)
		cfg = &tls.Config{RootCAs: roots, ServerName: "localhost", MinVersion: tls.VersionTLS13, NextProtos: []string{"http/1.1"}}
	}
	dial := func() (net.Conn, error) {
		if cfg != nil {
			c, err := tls.Dial("tcp", *addr, cfg)
			if err != nil {
				return nil, err
			}
			return c, c.Handshake()
		}
		return net.Dial("tcp", *addr)
	}
	var total, errors int64
	var mu sync.Mutex
	var lat []time.Duration
	var wg sync.WaitGroup
	// https: the connections and their handshakes before the clock (the
	// handshake is not what is measured); http: each dialed inside the
	// measured time, as the columns before https were
	opened := make([]net.Conn, *conns)
	if cfg != nil {
		for i := range opened {
			wg.Add(1)
			go func(i int) {
				defer wg.Done()
				c, err := dial()
				if err != nil {
					atomic.AddInt64(&errors, 1)
					return
				}
				opened[i] = c
			}(i)
		}
		wg.Wait()
	}
	deadline := time.Now().Add(*dur)
	head := *method + " " + *path + " HTTP/1.1\r\nHost: localhost\r\n"
	if *bodyN > 0 || *method == "POST" || *method == "PUT" {
		head += "Content-Length: " + strconv.Itoa(*bodyN) + "\r\n"
	}
	req := append([]byte(head+"\r\n"), body...)
	for i := 0; i < *conns; i++ {
		if cfg != nil && opened[i] == nil {
			continue
		}
		wg.Add(1)
		go func(c net.Conn) {
			defer wg.Done()
			if c == nil {
				var err error
				if c, err = dial(); err != nil {
					atomic.AddInt64(&errors, 1)
					return
				}
			}
			defer c.Close()
			br := bufio.NewReaderSize(c, 4096)
			var my []time.Duration
			for time.Now().Before(deadline) {
				t0 := time.Now()
				if _, err := c.Write(req); err != nil {
					atomic.AddInt64(&errors, 1)
					return
				}
				resp, err := http.ReadResponse(br, nil)
				if err != nil {
					atomic.AddInt64(&errors, 1)
					return
				}
				io.Copy(io.Discard, resp.Body)
				resp.Body.Close()
				if resp.StatusCode != 200 {
					atomic.AddInt64(&errors, 1)
				}
				my = append(my, time.Since(t0))
				atomic.AddInt64(&total, 1)
			}
			mu.Lock()
			lat = append(lat, my...)
			mu.Unlock()
		}(opened[i])
	}
	wg.Wait()
	sort.Slice(lat, func(i, j int) bool { return lat[i] < lat[j] })
	p := func(q float64) time.Duration {
		if len(lat) == 0 {
			return 0
		}
		return lat[int(float64(len(lat)-1)*q)]
	}
	fmt.Printf("%s c=%d: %.0f req/s, errors %d, p50 %v, p99 %v\n", *addr, *conns, float64(total)/dur.Seconds(), errors, p(0.5), p(0.99))
}

// HTTP/2 through net/http: the Transport's streams, one request in flight
// per goroutine
func loadH2(addr string, conns int, dur time.Duration, ca, method, path string, body []byte) {
	p := new(http.Protocols)
	tr := &http.Transport{MaxIdleConnsPerHost: conns}
	scheme := "http"
	if ca != "" {
		pem, err := os.ReadFile(ca)
		if err != nil {
			fmt.Println("ca:", err)
			os.Exit(1)
		}
		roots := x509.NewCertPool()
		roots.AppendCertsFromPEM(pem)
		tr.TLSClientConfig = &tls.Config{RootCAs: roots, ServerName: "localhost", MinVersion: tls.VersionTLS13}
		p.SetHTTP2(true)
		scheme = "https"
	} else {
		p.SetUnencryptedHTTP2(true)
	}
	tr.Protocols = p
	client := &http.Client{Transport: tr}
	url := scheme + "://" + addr + path
	do := func() error {
		var rd io.Reader
		if len(body) > 0 {
			rd = bytes.NewReader(body)
		}
		req, err := http.NewRequest(method, url, rd)
		if err != nil {
			return err
		}
		resp, err := client.Do(req)
		if err != nil {
			return err
		}
		io.Copy(io.Discard, resp.Body)
		resp.Body.Close()
		if resp.StatusCode != 200 || resp.ProtoMajor != 2 {
			return fmt.Errorf("status %d proto %s", resp.StatusCode, resp.Proto)
		}
		return nil
	}
	if err := do(); err != nil { // the connection and its handshake before the clock
		fmt.Println("first request:", err)
		os.Exit(1)
	}
	var total, errors int64
	var mu sync.Mutex
	var lat []time.Duration
	var wg sync.WaitGroup
	deadline := time.Now().Add(dur)
	for i := 0; i < conns; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			var my []time.Duration
			for time.Now().Before(deadline) {
				t0 := time.Now()
				if err := do(); err != nil {
					atomic.AddInt64(&errors, 1)
					continue
				}
				my = append(my, time.Since(t0))
				atomic.AddInt64(&total, 1)
			}
			mu.Lock()
			lat = append(lat, my...)
			mu.Unlock()
		}()
	}
	wg.Wait()
	sort.Slice(lat, func(i, j int) bool { return lat[i] < lat[j] })
	q := func(f float64) time.Duration {
		if len(lat) == 0 {
			return 0
		}
		return lat[int(float64(len(lat)-1)*f)]
	}
	fmt.Printf("%s c=%d: %.0f req/s, errors %d, p50 %v, p99 %v\n", addr, conns, float64(total)/dur.Seconds(), errors, q(0.5), q(0.99))
}
