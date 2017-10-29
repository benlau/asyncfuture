#include <asyncfuture.h>
using namespace AsyncFuture;

static void case1() {
    // More than 1 argument in callback function
    auto defer = Deferred<int>();

    observe(defer.future()).subscribe([=](int a, int b) {
        Q_UNUSED(a);
        Q_UNUSED(b);
    });

}

static void case2() {
    auto defer = Deferred<void>();

    observe(defer.future()).subscribe([=](int a) {
        Q_UNUSED(a);
    });
}
