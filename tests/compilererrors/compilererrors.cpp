#include <asyncfuture.h>
using namespace AsyncFuture;

//@FIXME - validate context function

/** static_assert() should be able to capture all kind of compiler errors in following conditions,
 * instead of reporting confusing compiler errors
 */

#define ENABLED

#if defined(ENABLED)
// More than 1 argument in callback function
static void case1a() {
    auto defer = Deferred<int>();

    observe(defer.future()).subscribe([=](int a, int b) {
        Q_UNUSED(a);
        Q_UNUSED(b);
    });

}
#endif

#if defined(ENABLED)
static void case1b() {
    auto defer = Deferred<int>();

    observe(defer.future()).subscribe([=](int a, int b) {
        Q_UNUSED(a);
        Q_UNUSED(b);
        return 99;
    });
}
#endif

#if defined(ENABLED)
static void case2() {
    auto defer = Deferred<void>();

    observe(defer.future()).subscribe([=](int a) {
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
