// Same shape as benchmarks/weak_ptr.cpp: the cost of a weak reference
// (java.lang.ref.WeakReference).
//   java WeakPtr [threads=1] [op=lock|copy|make|expired]
//   lock: a weak reference to a live object dereferenced (get)
//   expired: a weak reference to a collected object dereferenced: the null answer
//   copy: the reference copied into a local, then tested
//   make: a WeakReference made from a strong reference, then tested
// Prints nanoseconds per operation.
import java.lang.ref.WeakReference;

public final class WeakPtr {
    static final class Node { long v; long p0, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14; }

    static final long ITERS = 20_000_000L;
    static final long[] sink = new long[256 * 16];

    public static void main(String[] args) throws Exception {
        int threads = args.length > 0 ? Integer.parseInt(args[0]) : 1;
        String op = args.length > 1 ? args[1] : "lock";
        Thread[] ws = new Thread[threads];
        long t0 = System.nanoTime();
        for (int t = 0; t < threads; ++t) {
            final int id = t;
            ws[t] = new Thread(() -> {
                Node strong = new Node();
                strong.v = 1;
                WeakReference<Node> weak = new WeakReference<>(strong);
                if (op.equals("expired")) {
                    strong = new Node();   // the first node dropped: collected below, the reference cleared
                    strong.v = 1;
                    for (int i = 0; i < 10 && weak.get() != null; ++i) { System.gc(); }
                    if (weak.get() != null) System.err.println("no object expired: the numbers below are of a live one");
                }
                long sum = 0;
                switch (op) {
                    case "lock":
                    case "expired":
                        for (long i = 0; i < ITERS; ++i) { Node p = weak.get(); if (p != null) sum += p.v; }
                        break;
                    case "copy":
                        for (long i = 0; i < ITERS; ++i) { WeakReference<Node> copy = weak; sum += copy.get() == null ? 1 : 0; }
                        break;
                    default:
                        for (long i = 0; i < ITERS; ++i) { WeakReference<Node> made = new WeakReference<>(strong); sum += made.get() == null ? 1 : 0; }
                        break;
                }
                sink[id * 16] = sum + strong.v;
            });
            ws[t].start();
        }
        for (Thread w : ws) w.join();
        double ns = (System.nanoTime() - t0) / (double) ITERS;
        System.out.printf("java threads=%d op=%s ns/op=%.2f cpu=%.2fs%n", threads, op, ns, Common.cpuSeconds());
    }
}
