#include <asyncfuture.h>
using namespace AsyncFuture;

//@FIXME - validate context function

/** static_assert() should be able to capture all kind of compiler errors in following conditions,
 * instead of reporting confusing compiler errors
 */

#define ENABLED

// Case 1 - More than 1 argument in callback function

#if defined(ENABLED)
static void case1a() { // subscribe - no return
    auto defer = Deferred<int>();

    observe(defer.future()).subscribe([=](int a, int b) {
        Q_UNUSED(a);
        Q_UNUSED(b);
    });

}
#endif

#if defined(ENABLED)
static void case1b() {  // subscribe - with return
    auto defer = Deferred<int>();

    observe(defer.future()).subscribe([=](int a, int b) {
        Q_UNUSED(a);
        Q_UNUSED(b);
        return 99;
    });
}
#endif

#if defined(ENABLED)
static void case1c() {  // context - with return
    auto defer = Deferred<int>();

    QObject tmp;

    observe(defer.future()).context(&tmp, [=](int a, int b) {
        Q_UNUSED(a);
        Q_UNUSED(b);
        return 99;
    });
}
#endif

// Case 1 - observe a QFuture<void> by a callback with input argument
#if defined(ENABLED)
static void case2a() {
    auto defer = Deferred<void>();

    observe(defer.future()).subscribe([=](int a) {
        Q_UNUSED(a);
    });
}
#endif

#if defined(ENABLED)
static void case2b() {
    auto defer = Deferred<void>();
    QObject tmp;

    observe(defer.future()).context(&tmp, [=](int a) {
        Q_UNUSED(a);
    });
}
#endif

#if defined(ENABLED)
static void case3() {
    //@FIXME - the static_assert error message is missing

    auto defer = Deferred<QString>();

    observe(defer.future()).subscribe([=](int a) {
        Q_UNUSED(a);
    });

}
#endif
