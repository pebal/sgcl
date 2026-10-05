// The proxies of the interop tests, in Go, on the loopback (built by the
// tests with `go build`; skipped where there is no go):
//
//	go_proxy socks5 [user pass]        a SOCKS5 server (RFC 1928, RFC 1929); prints "port N"
//	go_proxy http [user pass]          an HTTP proxy: absolute-form requests forwarded,
//	                                   CONNECT tunnelled (RFC 9110 §9.3.6); prints "port N"
//	go_proxy get PROXY URL [CA]        Go's net/http client through PROXY (http://, https://,
//	                                   socks5://): prints "STATUS BODY"
//
// A server ends at a request for the host quit.invalid, or after two
// minutes, so that a test that failed half-way leaves nothing running.
package main

import (
	"bufio"
	"crypto/tls"
	"crypto/x509"
	"encoding/base64"
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"sync/atomic"
	"time"
)

func listen() net.Listener {
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	go func() {
		time.Sleep(2 * time.Minute)
		os.Exit(0)
	}()
	return l
}

func relay(a, b net.Conn) {
	done := make(chan struct{}, 2)
	cp := func(dst, src net.Conn) {
		io.Copy(dst, src)
		if t, ok := dst.(*net.TCPConn); ok {
			t.CloseWrite()
		}
		done <- struct{}{}
	}
	go cp(a, b)
	go cp(b, a)
	<-done
	<-done
	a.Close()
	b.Close()
}

// --- SOCKS5 ------------------------------------------------------------------

func socks5(user, pass string) {
	l := listen()
	for {
		c, err := l.Accept()
		if err != nil {
			return
		}
		go socks5Conn(c, user, pass)
	}
}

func socks5Conn(c net.Conn, user, pass string) {
	r := bufio.NewReader(c)
	head := make([]byte, 2)
	if _, err := io.ReadFull(r, head); err != nil || head[0] != 5 {
		c.Close()
		return
	}
	methods := make([]byte, head[1])
	if _, err := io.ReadFull(r, methods); err != nil {
		c.Close()
		return
	}
	want := byte(0)
	if user != "" {
		want = 2
	}
	chosen := byte(0xFF)
	for _, m := range methods {
		if m == want {
			chosen = want
		}
	}
	c.Write([]byte{5, chosen})
	if chosen == 0xFF {
		c.Close()
		return
	}
	if chosen == 2 {
		v := make([]byte, 2)
		if _, err := io.ReadFull(r, v); err != nil {
			c.Close()
			return
		}
		u := make([]byte, v[1])
		io.ReadFull(r, u)
		pl := make([]byte, 1)
		io.ReadFull(r, pl)
		p := make([]byte, pl[0])
		io.ReadFull(r, p)
		if string(u) != user || string(p) != pass {
			c.Write([]byte{1, 1})
			c.Close()
			return
		}
		c.Write([]byte{1, 0})
	}
	req := make([]byte, 4)
	if _, err := io.ReadFull(r, req); err != nil {
		c.Close()
		return
	}
	var host string
	switch req[3] {
	case 1:
		a := make([]byte, 4)
		io.ReadFull(r, a)
		host = net.IP(a).String()
	case 4:
		a := make([]byte, 16)
		io.ReadFull(r, a)
		host = net.IP(a).String()
	case 3:
		n := make([]byte, 1)
		io.ReadFull(r, n)
		a := make([]byte, n[0])
		io.ReadFull(r, a)
		host = string(a)
	default:
		c.Write([]byte{5, 8, 0, 1, 0, 0, 0, 0, 0, 0})
		c.Close()
		return
	}
	pb := make([]byte, 2)
	io.ReadFull(r, pb)
	port := binary.BigEndian.Uint16(pb)
	if host == "quit.invalid" {
		os.Exit(0)
	}
	if req[1] != 1 {
		c.Write([]byte{5, 7, 0, 1, 0, 0, 0, 0, 0, 0})
		c.Close()
		return
	}
	t, err := net.DialTimeout("tcp", net.JoinHostPort(host, strconv.Itoa(int(port))), 5*time.Second)
	if err != nil {
		rep := byte(4)
		if strings.Contains(err.Error(), "refused") {
			rep = 5
		}
		c.Write([]byte{5, rep, 0, 1, 0, 0, 0, 0, 0, 0})
		c.Close()
		return
	}
	bound := t.LocalAddr().(*net.TCPAddr)
	reply := []byte{5, 0, 0, 1}
	reply = append(reply, bound.IP.To4()...)
	reply = binary.BigEndian.AppendUint16(reply, uint16(bound.Port))
	c.Write(reply)
	if n := r.Buffered(); n > 0 {
		b, _ := r.Peek(n)
		t.Write(b)
	}
	relay(c, t)
}

// --- HTTP --------------------------------------------------------------------

var forwarded atomic.Int64

func authorized(r *http.Request, user, pass string) bool {
	if user == "" {
		return true
	}
	h := r.Header.Get("Proxy-Authorization")
	if !strings.HasPrefix(h, "Basic ") {
		return false
	}
	b, err := base64.StdEncoding.DecodeString(h[6:])
	return err == nil && string(b) == user+":"+pass
}

type proxy struct {
	user, pass string
	transport  *http.Transport
}

func (p *proxy) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	host := r.Host
	if h, _, err := net.SplitHostPort(host); err == nil {
		host = h
	}
	if host == "quit.invalid" {
		os.Exit(0)
	}
	if !authorized(r, p.user, p.pass) {
		w.Header().Set("Proxy-Authenticate", `Basic realm="go_proxy"`)
		w.WriteHeader(http.StatusProxyAuthRequired)
		return
	}
	if r.Method == http.MethodConnect {
		t, err := net.DialTimeout("tcp", r.Host, 5*time.Second)
		if err != nil {
			http.Error(w, err.Error(), http.StatusBadGateway)
			return
		}
		h, ok := w.(http.Hijacker)
		if !ok {
			t.Close()
			return
		}
		c, buf, err := h.Hijack()
		if err != nil {
			t.Close()
			return
		}
		c.Write([]byte("HTTP/1.1 200 Connection established\r\n\r\n"))
		if n := buf.Reader.Buffered(); n > 0 {
			b, _ := buf.Reader.Peek(n)
			t.Write(b)
		}
		relay(c, t)
		return
	}
	if !r.URL.IsAbs() {
		http.Error(w, "not a proxy request", http.StatusBadRequest)
		return
	}
	forwarded.Add(1)
	out := r.Clone(r.Context())
	out.RequestURI = ""
	out.Header.Del("Proxy-Authorization")
	out.Header.Del("Proxy-Connection")
	res, err := p.transport.RoundTrip(out)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	defer res.Body.Close()
	for k, v := range res.Header {
		w.Header()[k] = v
	}
	w.Header().Set("X-Forwarded-By", "go_proxy")
	w.WriteHeader(res.StatusCode)
	io.Copy(w, res.Body)
}

func httpProxy(user, pass string) {
	l := listen()
	p := &proxy{user: user, pass: pass, transport: &http.Transport{Proxy: nil}}
	(&http.Server{Handler: p}).Serve(l)
}

// --- Go's client through a proxy ---------------------------------------------

func get(proxyURL, target, ca string) {
	pu, err := url.Parse(proxyURL)
	if err != nil {
		fmt.Println("bad proxy:", err)
		return
	}
	tr := &http.Transport{Proxy: http.ProxyURL(pu)}
	if ca != "" {
		pem, err := os.ReadFile(ca)
		if err != nil {
			fmt.Println(err)
			return
		}
		pool := x509.NewCertPool()
		pool.AppendCertsFromPEM(pem)
		tr.TLSClientConfig = &tls.Config{RootCAs: pool}
		tr.ForceAttemptHTTP2 = true // a TLS config of its own turns HTTP/2 off otherwise
	}
	c := &http.Client{Transport: tr, Timeout: 10 * time.Second}
	res, err := c.Get(target)
	if err != nil {
		fmt.Println("error:", err)
		return
	}
	b, _ := io.ReadAll(res.Body)
	res.Body.Close()
	fmt.Printf("%d %s", res.StatusCode, b)
}

func main() {
	arg := func(i int) string {
		if len(os.Args) > i {
			return os.Args[i]
		}
		return ""
	}
	switch arg(1) {
	case "socks5":
		socks5(arg(2), arg(3))
	case "http":
		httpProxy(arg(2), arg(3))
	case "get":
		get(arg(2), arg(3), arg(4))
	default:
		fmt.Fprintln(os.Stderr, "usage: go_proxy socks5|http [user pass] | get PROXY URL [CA]")
		os.Exit(2)
	}
}
