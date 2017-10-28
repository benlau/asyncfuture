#include <asyncfuture.h>
using namespace AsyncFuture;

static void demo() {
    auto defer = Deferred<void>();

    observe(defer.future()).subscribe([=](int a, int b) {
        Q_UNUSED(a);
        Q_UNUSED(b);
    });

}
