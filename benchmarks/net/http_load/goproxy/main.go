package main

// Go's side of the reverse proxy load test (run_proxy.sh): httputil's
// ReverseProxy to one backend (gosrv), its transport keeping as many idle
// connections a host as the module's proxy does (256; Go's default is 2),
// no proxy of the environment, as the module's default.
//
//	goproxy [address=:18080 [backend=http://127.0.0.1:18081]]
import (
	"net/http"
	"net/http/httputil"
	"net/url"
	"os"
)

func main() {
	addr := ":18080"
	if len(os.Args) > 1 {
		addr = os.Args[1]
	}
	backend := "http://127.0.0.1:18081"
	if len(os.Args) > 2 {
		backend = os.Args[2]
	}
	u, err := url.Parse(backend)
	if err != nil {
		panic(err)
	}
	p := httputil.NewSingleHostReverseProxy(u)
	t := http.DefaultTransport.(*http.Transport).Clone()
	t.Proxy = nil
	t.MaxIdleConns = 0
	t.MaxIdleConnsPerHost = 256
	p.Transport = t
	if err := http.ListenAndServe(addr, p); err != nil {
		panic(err)
	}
}
