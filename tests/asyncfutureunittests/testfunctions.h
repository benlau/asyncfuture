#ifndef TESTFUNCTIONS_H
#define TESTFUNCTIONS_H

#include <functional>
#include <Automator>
#include <QFuture>
#include <QTime>

namespace Test {

    template <typename T>
    class Callable {
    public:
        Callable() : called(false) {
            func = [=](T input) {
                called = true;
                value = input;
            };
        }
        bool called;
        T value;

        std::function<void(T)> func;
    };

    template <>
    class Callable<void> {
    public:
        Callable() : called(false) {
            func = [=]() {
                called = true;
            };
        }
        bool called;
        std::function<void()> func;
    };

    inline void tick() {
        int i = 10;
        while (i--) {
            Automator::wait(0);
        }
    }

    template <typename F>
    bool waitUntil(F f, int timeout = -1) {
        QTime time;
        time.start();

        while (!f()) {
            Automator::wait(10);
            if (timeout > 0 && time.elapsed() > timeout) {
                return false;
            }
        }
        tick();
        return true;
    }

    template <typename T>
    bool waitUntil(QFuture<T> future, int timeout = -1) {
        return waitUntil([=]() {
           return future.isFinished();
        }, timeout);
    }

}

#endif // TESTFUNCTIONS_H
