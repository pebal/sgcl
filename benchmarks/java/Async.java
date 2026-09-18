// The async module's counterparts in Java: virtual threads (the JDK's
// scheduler), CompletableFuture.orTimeout, SynchronousQueue, Condition
// and ReentrantLock, one case per run (benchmarks/async/async.cpp has
// the SGCL side; the cases with no counterpart here, an executor's
// yield, a strand, a select and a generator, are left out). Prints one
// line, ns per operation.
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.SynchronousQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public final class Async {
    static long sum;

    static void report(String what, double wall, long ops) {
        System.out.printf("async %s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%n", what, wall * 1e9 / ops, ops / wall, wall, Common.cpuSeconds());
    }

    static double seconds(long t0) {
        return (System.nanoTime() - t0) / 1e9;
    }

    public static void main(String[] args) throws Exception {
        String what = args.length > 0 ? args[0] : "yield";
        long n = args.length > 1 ? Long.parseLong(args[1]) : 0;
        switch (what) {
            case "yield" -> {   // Thread.yield in a virtual thread
                long m = n != 0 ? n : 2_000_000;
                long t0 = System.nanoTime();
                Thread t = Thread.startVirtualThread(() -> { for (long i = 0; i < m; ++i) Thread.yield(); });
                t.join();
                report("yield", seconds(t0), m);
            }
            case "await", "spawn" -> {   // a virtual thread started and joined, one at a time, from a virtual thread
                long m = n != 0 ? n : 500_000;
                long t0 = System.nanoTime();
                Thread outer = Thread.startVirtualThread(() -> {
                    try {
                        for (long i = 0; i < m; ++i) {
                            final long v = i;
                            long[] r = new long[1];
                            Thread t = Thread.startVirtualThread(() -> r[0] = v);
                            t.join();
                            sum += r[0];
                        }
                    } catch (InterruptedException e) { throw new RuntimeException(e); }
                });
                outer.join();
                report(what, seconds(t0), m);
            }
            case "whenall" -> {   // two virtual threads joined, per thread
                long m = n != 0 ? n : 500_000;
                long t0 = System.nanoTime();
                Thread outer = Thread.startVirtualThread(() -> {
                    try {
                        for (long i = 0; i < m; ++i) {
                            final long v = i;
                            long[] r = new long[2];
                            Thread a = Thread.startVirtualThread(() -> r[0] = v);
                            Thread b = Thread.startVirtualThread(() -> r[1] = v);
                            a.join();
                            b.join();
                            sum += r[0] + r[1];
                        }
                    } catch (InterruptedException e) { throw new RuntimeException(e); }
                });
                outer.join();
                report("whenall", seconds(t0), 2 * m);
            }
            case "timeout" -> {   // a task that answers at once, raced against a timeout of an hour: CompletableFuture.orTimeout
                long m = n != 0 ? n : 200_000;
                long t0 = System.nanoTime();
                Thread outer = Thread.startVirtualThread(() -> {
                    for (long i = 0; i < m; ++i) {
                        final long v = i;
                        CompletableFuture<Long> f = new CompletableFuture<>();
                        Thread.startVirtualThread(() -> f.complete(v));
                        sum += f.orTimeout(1, TimeUnit.HOURS).join();
                    }
                });
                outer.join();
                report("timeout", seconds(t0), m);
            }
            case "cv" -> {   // a turn handed between two virtual threads through a Condition
                long m = n != 0 ? n : 200_000;
                ReentrantLock lock = new ReentrantLock();
                Condition cv = lock.newCondition();
                int[] turn = {0};
                Runnable side0 = () -> side(lock, cv, turn, 0, m);
                Runnable side1 = () -> side(lock, cv, turn, 1, m);
                long t0 = System.nanoTime();
                Thread a = Thread.startVirtualThread(side0);
                Thread b = Thread.startVirtualThread(side1);
                a.join();
                b.join();
                report("cv", seconds(t0), 2 * m);
            }
            case "pingpong" -> {   // two virtual threads over two SynchronousQueues, per hop
                long m = n != 0 ? n : 500_000;
                SynchronousQueue<Long> a = new SynchronousQueue<>(), b = new SynchronousQueue<>();
                long t0 = System.nanoTime();
                Thread p = Thread.startVirtualThread(() -> {
                    try { for (long i = 0; i < m; ++i) { a.put(i); b.take(); } } catch (InterruptedException e) { throw new RuntimeException(e); }
                });
                Thread q = Thread.startVirtualThread(() -> {
                    try { for (long i = 0; i < m; ++i) { a.take(); b.put(i); } } catch (InterruptedException e) { throw new RuntimeException(e); }
                });
                p.join();
                q.join();
                report("pingpong", seconds(t0), 2 * m);
            }
            case "mutex" -> {   // ReentrantLock, uncontended, in a virtual thread
                long m = n != 0 ? n : 2_000_000;
                ReentrantLock lock = new ReentrantLock();
                long t0 = System.nanoTime();
                Thread t = Thread.startVirtualThread(() -> { for (long i = 0; i < m; ++i) { lock.lock(); sum += i; lock.unlock(); } });
                t.join();
                report("mutex", seconds(t0), m);
            }
            default -> {
                System.err.println("unknown case " + what);
                System.exit(2);
            }
        }
        if (sum == -1) {
            System.out.print("?");
        }
    }

    static void side(ReentrantLock lock, Condition cv, int[] turn, int mine, long n) {
        for (long i = 0; i < n; ++i) {
            lock.lock();
            try {
                while (turn[0] % 2 != mine) {
                    cv.awaitUninterruptibly();
                }
                ++turn[0];
                cv.signal();
            } finally {
                lock.unlock();
            }
        }
    }
}
