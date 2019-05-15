/* AsyncFuture Version: 0.4.1 */
#pragma once
#include <QFuture>
#include <QMetaMethod>
#include <QPointer>
#include <QThread>
#include <QFutureWatcher>
#include <QCoreApplication>
#include <QMutex>
#include <functional>

#define ASYNCFUTURE_ERROR_OBSERVE_VOID_WITH_ARGUMENT "Observe a QFuture<void> but your callback contains an input argument"
#define ASYNCFUTURE_ERROR_CALLBACK_NO_MORE_ONE_ARGUMENT "Callback function should not take more than 1 argument"
#define ASYNCFUTURE_ERROR_ARGUMENT_MISMATCHED "The callback function is not callable. The input argument doesn't match with the observing QFuture type"

#define ASYNC_FUTURE_CALLBACK_STATIC_ASSERT(Tag, Completed) \
    static_assert(Private::arg_count<Completed>::value <= 1, Tag ASYNCFUTURE_ERROR_CALLBACK_NO_MORE_ONE_ARGUMENT); \
    static_assert(!(std::is_same<void, T>::value && Private::arg_count<Completed>::value >= 1), Tag ASYNCFUTURE_ERROR_OBSERVE_VOID_WITH_ARGUMENT); \
    static_assert( Private::decay_is_same<T, Private::Arg0Type<Completed>>::value || \
                   Private::arg0_is_future<Completed>::value || \
                   std::is_same<void, Private::Arg0Type<Completed>>::value, Tag ASYNCFUTURE_ERROR_ARGUMENT_MISMATCHED);


namespace AsyncFuture {

/* Naming Convention
 *
 * typename T - The type of observable QFuture
 * typename R - The return type of callback
 */

namespace Private {

/* Begin traits functions */

// Determine is the input type a QFuture
template <typename T>
struct future_traits {
    enum {
        is_future = 0
    };

    typedef void arg_type;
};

template <template <typename> class C, typename T>
struct future_traits<C <T> >
{
    enum {
        is_future = 0
    };

    typedef void arg_type;
};

template <typename T>
struct future_traits<QFuture<QFuture<T>>> : public future_traits<QFuture<T>> {
};

template <typename T>
struct future_traits<QFuture<T> >{
    enum {
        is_future = 1
    };
    typedef T arg_type;
};

// function_traits: Source: http://stackoverflow.com/questions/7943525/is-it-possible-to-figure-out-the-parameter-type-and-return-type-of-a-lambda

template <typename T>
struct function_traits
        : public function_traits<decltype(&T::operator())>
{};

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...) const>
// we specialize for pointers to member function
{
    enum { arity = sizeof...(Args) };
    // arity is the number of arguments.

    typedef ReturnType result_type;

    enum {
        result_type_is_future = future_traits<result_type>::is_future
    };

    // If the result_type is a QFuture<T>, the type will be T. Otherwise, it is void
    typedef typename future_traits<result_type>::arg_type future_arg_type;

    template <size_t i>
    struct arg
    {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };
};

/* It is an additional to the original function_traits to handle non-const function (with mutable keyword lambda). */

template <typename ClassType, typename ReturnType, typename... Args>
struct function_traits<ReturnType(ClassType::*)(Args...)>
// we specialize for pointers to member function
{
    enum { arity = sizeof...(Args) };
    // arity is the number of arguments.

    typedef ReturnType result_type;

    enum {
        result_type_is_future = future_traits<result_type>::is_future
    };

    // If the result_type is a QFuture<T>, the type will be T. Otherwise, it is void
    typedef typename future_traits<result_type>::arg_type future_arg_type;

    template <size_t i>
    struct arg
    {
        typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
        // the i-th argument is equivalent to the i-th tuple element of a tuple
        // composed of those arguments.
    };
};

/// Decay 2nd parameter and check is it same as the first argument
template <typename T, typename U>
struct decay_is_same :
    std::is_same<T, typename std::decay<U>::type>::type
{};

template <typename T>
struct signal_traits {
    // Match class member function only
};

template <typename R, typename C>
struct signal_traits<R (C::*)()> {
    typedef void result_type;
};

template <typename R, typename C, typename ARG0>
struct signal_traits<R (C::*)(ARG0)> {
    typedef ARG0 result_type;
};

template <typename T>
struct arg0_traits : public arg0_traits<decltype(&T::operator())> {
};

template <typename C, typename R>
struct arg0_traits<R(C::*)() const> {
    typedef void type;
};

template <typename C, typename R>
struct arg0_traits<R(C::*)()> {
    typedef void type;
};

template <typename C, typename R, typename Arg0, typename ...Args>
struct arg0_traits<R(C::*)(Arg0, Args...) const> {
    typedef Arg0 type;
};

template <typename C, typename R, typename Arg0, typename ...Args>
struct arg0_traits<R(C::*)(Arg0, Args...)> {
    typedef Arg0 type;
};
    
template <typename R>
struct arg0_traits<R()> {
  typedef void type;
};

template <typename R, typename Arg0, typename ...Args>
struct arg0_traits<R(Arg0, Args...)> {
  typedef Arg0 type;
};

// Obtain the observable type according to the Functor
template <typename T>
struct observable_traits: public observable_traits<decltype(&T::operator())> {
};

template <typename C, typename R, typename ...Args>
struct observable_traits<QFuture<QFuture<R>>(C::*)(Args...) const> {
    typedef R type;
};

template <typename C, typename R, typename ...Args>
struct observable_traits<QFuture<QFuture<R>>(C::*)(Args...)> {
    typedef R type;
};

template <typename C, typename R, typename ...Args>
struct observable_traits<QFuture<R>(C::*)(Args...) const> {
    typedef R type;
};

template <typename C, typename R, typename ...Args>
struct observable_traits<QFuture<R>(C::*)(Args...)> {
    typedef R type;
};

template <typename C, typename R, typename ...Args>
struct observable_traits<R(C::*)(Args...) const> {
    typedef R type;
};

template <typename C, typename R, typename ...Args>
struct observable_traits<R(C::*)(Args...)> {
    typedef R type;
};

template <typename Functor>
using RetType = typename function_traits<Functor>::result_type;

template <typename Functor>
using Arg0Type = typename arg0_traits<Functor>::type;

template <typename Functor>
struct ret_type_is_void {
    enum {
        value = std::is_same<RetType<Functor>, void>::value
    };
};

template <typename Functor>
struct ret_type_is_future {
    enum {
        value = future_traits<RetType<Functor>>::is_future
    };
};

template <typename Functor>
struct arg0_is_future {
    enum {
        value = future_traits<typename std::decay<Arg0Type<Functor>>::type>::is_future
    };
};

template <typename Functor>
struct arg_count_is_zero {
    enum {
        value = (function_traits<Functor>::arity == 0)
    };
};

template <typename Functor>
struct arg_count_is_not_zero {
    enum {
        value = (function_traits<Functor>::arity > 0)
    };
};

template <typename Functor>
struct arg_count_is_one {
    enum {
        value = (function_traits<Functor>::arity == 1)
    };
};

template <typename Functor>
struct arg_count {
    enum {
        value = function_traits<Functor>::arity
    };
};


template<typename T>
struct False : std::false_type {
};

template< typename Functor, typename T>
struct is_callable {
    enum {
        value = (arg_count<Functor>::value == 1) &&
                (!std::is_same<void, T>::value) &&
                (std::is_convertible<Arg0Type<Functor>, T>::value)
    };
};

/* End of traits functions */


// Value is a wrapper of a data structure which could contain <void> type.
// AsyncFuture does not use QVariant because it needs user to register before use.
template <typename R>
class Value {
public:
    Value() {
    }

    Value(R&& v) : value(v){
    }

    Value(R* v) : value(*v) {
    }

    Value(QFuture<R> future) {
        value = future.result();
    }


    R value;
};

template <>
class Value<void> {
public:
    Value() {
    }

    Value(void*) {
    }

    Value(QFuture<void> future) {
        Q_UNUSED(future);
    }
};

template <typename F>
void runInMainThread(F func) {
    QObject tmp;
    QObject::connect(&tmp, &QObject::destroyed,
                     QCoreApplication::instance(), std::move(func), Qt::QueuedConnection);
}

/*
 * @param owner If the object is destroyed, it should destroy the watcher
 * @param contextObject Determine the receiver callback
 */

template <typename T, typename Finished, typename Canceled>
void watch(QFuture<T> future,
           QObject* owner,
           QObject* contextObject,
           Finished finished,
           Canceled canceled) {

    Q_ASSERT(owner);
    QPointer<QObject> ownerAlive = owner;

    QFutureWatcher<T> *watcher = new QFutureWatcher<T>();

    if (owner) {
        // Don't set parent as the context object as it may live in different thread
        QObject::connect(owner, &QObject::destroyed,
                         watcher, &QObject::deleteLater);
    }

    if (contextObject) {

        QObject::connect(watcher, &QFutureWatcher<T>::finished,
                         contextObject, [=]() {
            delete watcher;
            if (ownerAlive.isNull()) {
                return;
            }
            finished();
        });

        QObject::connect(watcher, &QFutureWatcher<T>::canceled,
                         contextObject, [=]() {
            delete watcher;
            if (ownerAlive.isNull()) {
                return;
            }
            canceled();
        });

    } else {

        QObject::connect(watcher, &QFutureWatcher<T>::finished,
                         [=]() {
            delete watcher;
            if (ownerAlive.isNull()) {
                return;
            }
            finished();
        });

        QObject::connect(watcher, &QFutureWatcher<T>::canceled,
                         [=]() {
            delete watcher;
            if (ownerAlive.isNull()) {
                return;
            }
            canceled();
        });
    }

    if ((QThread::currentThread() != QCoreApplication::instance()->thread()) &&
         (contextObject == 0 || QThread::currentThread() != contextObject->thread())) {
        // Move watcher to main thread if context object is not set.
        watcher->moveToThread(QCoreApplication::instance()->thread());
    }

    watcher->setFuture(future);
}

/* DeferredFuture implements a QFutureInterface that could complete/cancel a QFuture.
 *
 * 1) It is a private class that won't export to public
 *
 * 2) Its member function do not use <T> to avoid to use template specialization to handle <void>. Type checking should be done by user classes (e.g Deferred)
 *
 */

template <typename T>
class DeferredFuture : public QObject, public QFutureInterface<T>{
public:

    ~DeferredFuture() {
        cancel();
    }

    template <typename ANY>
    void track(QFuture<ANY> future) {
        QPointer<DeferredFuture<T>> thiz = this;
        QFutureWatcher<ANY> *watcher = new QFutureWatcher<ANY>();

        if ((QThread::currentThread() != QCoreApplication::instance()->thread())) {
            watcher->moveToThread(QCoreApplication::instance()->thread());
        }

        QObject::connect(watcher, &QFutureWatcher<ANY>::finished, [=]() {
            watcher->disconnect();
            watcher->deleteLater();
        });

        QObject::connect(watcher, &QFutureWatcher<ANY>::progressValueChanged, this, [=](int value) {
            if (thiz.isNull()) {
                return;
            }
            thiz->setProgressValue(value);
        });

        QObject::connect(watcher, &QFutureWatcher<ANY>::progressRangeChanged, this, [=](int min, int max) {
            if (thiz.isNull()) {
                return;
            }
            thiz->setProgressRange(min, max);
        });

        QObject::connect(watcher, &QFutureWatcher<ANY>::started, this, [=](){
            thiz->reportStarted();
        });

        QObject::connect(watcher, &QFutureWatcher<ANY>::paused, this, [=](){
            thiz->setPaused(true);
        });

        QObject::connect(watcher, &QFutureWatcher<ANY>::resumed, this, [=](){
            thiz->setPaused(false);
        });

        watcher->setFuture(future);

        QFutureInterface<T>::setProgressRange(future.progressMinimum(), future.progressMaximum());
        QFutureInterface<T>::setProgressValue(future.progressValue());

        if (future.isStarted()) {
            QFutureInterface<T>::reportStarted();
        }

        if (future.isPaused()) {
            QFutureInterface<T>::setPaused(true);
        }
    }

    bool isFinished() const {
        return QFutureInterface<T>::isFinished();
    }

    // complete<void>()
    void complete() {
        if (isFinished()) {
            return;
        }
        QFutureInterface<T>::reportFinished();
    }

    template <typename R>
    void complete(R value) {
        if (isFinished()) {
            return;
        }
        reportResult(value);
        QFutureInterface<T>::reportFinished();
    }

    template <typename R>
    void complete(QList<R>& value) {
        if (isFinished()) {
            return;
        }

        reportResult(value);
        QFutureInterface<T>::reportFinished();
    }

    template <typename R>
    void complete(Value<R> value) {
        this->complete(value.value);
    }

    void complete(Value<void> value) {
        Q_UNUSED(value);
        this->complete();
    }

    void complete(QFuture<T> future) {
        incWeakRefCount();
        auto onFinished = [=]() {
            this->completeByFinishedFuture<T>(future);
            this->decWeakRefCount();
        };

        auto onCanceled = [=]() {
            this->cancel();
            this->decWeakRefCount();
        };

        watch(future,
              this,
              nullptr,
              onFinished,
              onCanceled);

        track(future);
    }

    template <typename ANY>
    void complete(QFuture<QFuture<ANY>> future) {
        incWeakRefCount();

        auto onFinished = [=]() {
            complete(future.result());
            this->decWeakRefCount();
        };

        auto onCanceled = [=]() {
            this->cancel();
            this->decWeakRefCount();
        };

        watch(future,
              this,
              0,
              onFinished,
              onCanceled);
        // It don't track for the first level of future
    }

    void cancel() {
        if (isFinished()) {
            return;
        }
        QFutureInterface<T>::reportCanceled();
        QFutureInterface<T>::reportFinished();
    }

    template <typename Member>
    void cancel(QObject* sender, Member member) {
        incWeakRefCount();
        QObject::connect(sender, member,
                         this, [=]() {
            this->cancel();
            decWeakRefCount();
        });
    }

    template <typename ANY>
    void cancel(QFuture<ANY> future) {
        incWeakRefCount();
        auto onFinished = [=]() {
            cancel();
            decWeakRefCount();
        };

        auto onCanceled = [=]() {
            decWeakRefCount();
        };

        watch(future,
              this,
              0,
              onFinished,
              onCanceled);
    }

    void incWeakRefCount() {
        mutex.lock();
        refCount++;
        mutex.unlock();
    }

    void decWeakRefCount() {
        int count;

        mutex.lock();
        refCount--;
        count = refCount;
        mutex.unlock();

        if (count <= 0) {
            if (!isFinished()) {
                cancel();
            }
        }

        if (strongRefCount == 0 && isFinished()) {
            delete this;
        }
    }

    void incStrongRef() {
        mutex.lock();
        strongRefCount++;
        mutex.unlock();
    }

    void decStrongRef() {
        mutex.lock();
        strongRefCount--;
        mutex.unlock();
    }

    /// Create a DeferredFugture instance and manage by a shared pointer
    static QSharedPointer<DeferredFuture<T> > create() {

        auto deleter = [](DeferredFuture<T> *object) {
            object->decStrongRef();
            object->decWeakRefCount();
        };

        QSharedPointer<DeferredFuture<T> > ptr(new DeferredFuture<T>(), deleter);
        ptr->incStrongRef();
        return ptr;
    }

    template <typename R>
    void reportResult(R& value, int index = -1) {
        QFutureInterface<T>::reportResult(value, index);
    }

    void reportResult(Value<void> &value) {
        Q_UNUSED(value);
    }

    template <typename R>
    void reportResult(QList<R>& value) {
        for (int i = 0 ; i < value.size();i++) {
            QFutureInterface<T>::reportResult(value[i], i);
        }
    }

    template <typename R>
    void reportResult(Value<R>& value) {
        QFutureInterface<T>::reportResult(value.value);
    }

protected:
    DeferredFuture(QObject* parent = nullptr): QObject(parent),
                                         QFutureInterface<T>(QFutureInterface<T>::Running),
                                         refCount(1),
                                         strongRefCount(0) {
    }

    QMutex mutex;

private:

    // A reference count system. If it is dropped to zero, it will cancel this object
    int refCount;

    // Unless it is zero, this object will not be destroyed.
    int strongRefCount;

    /// The future is already finished. It will take effect immediately
    template <typename ANY>
    typename std::enable_if<!std::is_same<ANY,void>::value, void>::type
    completeByFinishedFuture(QFuture<T> future) {
        if (future.resultCount() > 1) {
            complete(future.results());
        } else if (future.resultCount() == 1) {
            complete(future.result());
        } else {
            complete();
        }
    }

    template <typename ANY>
    typename std::enable_if<std::is_same<ANY,void>::value, void>::type
    completeByFinishedFuture(QFuture<T> future) {
        Q_UNUSED(future);
        complete();
    }

protected:
};

class CombinedFuture: public DeferredFuture<void> {
public:
    CombinedFuture(bool settleAllModeArg = false) : DeferredFuture<void>(), settleAllMode(settleAllModeArg) {
        settledCount = 0;
        count = 0;
        anyCanceled = false;
    }

    template <typename T>
    void addFuture(const QFuture<T> future) {
        if (isFinished()) {
            return;
        }

        incWeakRefCount();

        mutex.lock();
        int index = count++;
        QFutureInterface<void>::setProgressRange(0, count);
        mutex.unlock();

        Private::watch(future, this, 0,
                       [=]() {
            completeFutureAt(index);
            decWeakRefCount();
        },[=]() {
            cancelFutureAt(index);
            decWeakRefCount();
        });
    }

    static QSharedPointer<CombinedFuture> create(bool settleAllMode) {

        auto deleter = [](CombinedFuture *object) {
            // Regardless of the no. of instance of QSharedPointer<CombinedFuture>,
            // it only increase the reference by one.
            object->decStrongRef();
            object->decWeakRefCount();
        };

        QSharedPointer<CombinedFuture> ptr(new CombinedFuture(settleAllMode), deleter);
        ptr->incStrongRef();
        return ptr;
    }

private:
    int settledCount;
    int count;
    bool anyCanceled;
    bool settleAllMode;

    void completeFutureAt(int index) {
        Q_UNUSED(index);
        mutex.lock();
        settledCount++;
        QFutureInterface<void>::setProgressValue(settledCount);
        mutex.unlock();
        checkFulfilled();
    }

    void cancelFutureAt(int index) {
        Q_UNUSED(index);

        mutex.lock();
        settledCount++;
        anyCanceled = true;
        QFutureInterface<void>::setProgressValue(settledCount);
        mutex.unlock();

        checkFulfilled();
    }

    void checkFulfilled() {
        if (isFinished()) {
            return;
        }

        if (anyCanceled && !settleAllMode) {
            cancel();
            return;
        }

        if (settledCount == count) {
            if (anyCanceled) {
                cancel();
            } else {
                complete();
            }
        }
    }

};

/// Proxy is a proxy class to connect a QObject signal to a callback function
template <typename ARG>
class Proxy : public QObject {
public:
    Proxy(QObject* parent) : QObject(parent) {
    }

    QVector<int> parameterTypes;
    std::function<void(Value<ARG>)> callback;
    QMetaObject::Connection conn;
    QPointer<QObject> sender;

    template <typename Method>
    void bind(QObject* source, Method pointToMemberFunction) {
        sender = source;

        const int memberOffset = QObject::staticMetaObject.methodCount();

        QMetaMethod method = QMetaMethod::fromSignal(pointToMemberFunction);

        parameterTypes = QVector<int>(method.parameterCount());

        for (int i = 0 ; i < method.parameterCount() ; i++) {
            parameterTypes[i] = method.parameterType(i);
        }

        conn = QMetaObject::connect(source, method.methodIndex(), this, memberOffset, Qt::QueuedConnection, 0);

        if (!conn) {
            qWarning() << "AsyncFuture::Private::Proxy: Failed to bind signal";
        }
    }

    int qt_metacall(QMetaObject::Call _c, int _id, void **_a) {
        int methodId = QObject::qt_metacall(_c, _id, _a);

        if (methodId < 0) {
            return methodId;
        }

        if (_c == QMetaObject::InvokeMetaMethod) {
            if (methodId == 0) {
                sender->disconnect(conn);
                if (parameterTypes.count() > 0) {
                    Value<ARG> value(reinterpret_cast<ARG*>(_a[1]));
                    callback(value);
                } else {
                    // It is triggered only if ARG==void.
                    callback(Value<ARG>((ARG*) 0));
                }
            }
        }
        return methodId;
    }
};

/// To bind a signal in const char* to callback
class Proxy2 : public QObject {
public:
    inline Proxy2(QObject* parent) : QObject(parent) {
    }

    QVector<int> parameterTypes;
    std::function<void(QVariant)> callback;
    QMetaObject::Connection conn;
    QPointer<QObject> sender;

    inline bool bind(QObject* source,QString signal) {
        sender = source;

        // Remove leading number
        signal = signal.replace(QRegExp("^[0-9]*"), "");

        const int memberOffset = QObject::staticMetaObject.methodCount();

        int index = source->metaObject()->indexOfSignal(signal.toUtf8().constData());

        if (index < 0) {
            qWarning() << "AsyncFuture::Private::Proxy: No such signal: " << signal;
            return false;
        }

        QMetaMethod method = source->metaObject()->method(index);

        parameterTypes = QVector<int>(method.parameterCount());

        for (int i = 0 ; i < method.parameterCount() ; i++) {
            parameterTypes[i] = method.parameterType(i);
        }

        conn = QMetaObject::connect(source, method.methodIndex(), this, memberOffset, Qt::QueuedConnection, 0);

        if (!conn) {
            qWarning() << "AsyncFuture::Private::Proxy: Failed to bind signal";
        }

        return true;
    }

    inline int qt_metacall(QMetaObject::Call _c, int _id, void **_a) {
        int methodId = QObject::qt_metacall(_c, _id, _a);

        if (methodId < 0) {
            return methodId;
        }

        if (_c == QMetaObject::InvokeMetaMethod) {
            if (methodId == 0) {
                sender->disconnect(conn);
                QVariant v;

                if (parameterTypes.count() > 0) {
                    const QMetaType::Type type = static_cast<QMetaType::Type>(parameterTypes.at(0));

                    if (type == QMetaType::QVariant) {
                        v = *reinterpret_cast<QVariant *>(_a[1]);
                    } else {
                        v = QVariant(type, _a[1]);
                    }
                }
                callback(v);
            }
        }
        return methodId;
    }

};

/* call() : Run functor(future):void */

template <typename Functor, typename T>
typename std::enable_if<is_callable<Functor, T>::value, void>::type
callIgnoreReturn(Functor& functor, QFuture<T>& future) {
    functor(future);
}

template <typename Functor, typename T>
typename std::enable_if<!is_callable<Functor, T>::value, void>::type
callIgnoreReturn(Functor& functor, QFuture<T>& future) {
    Q_UNUSED(functor);
    Q_UNUSED(future);
    /* Unlike clang, VC 2017 may not instantiate this function if another
     * static_assert is triggered.
     */
    static_assert(False<T>::value, ASYNCFUTURE_ERROR_ARGUMENT_MISMATCHED);
}

template <typename Functor, typename T>
typename std::enable_if<is_callable<Functor, T>::value, RetType<Functor>>::type
call(Functor& functor, QFuture<T>& future) {
    return functor(future);
}

template <typename Functor, typename T>
typename std::enable_if<!is_callable<Functor, T>::value, RetType<Functor>>::type
call(Functor& functor, QFuture<T>& future) {
    Q_UNUSED(functor);
    Q_UNUSED(future);
    static_assert(False<T>::value, ASYNCFUTURE_ERROR_ARGUMENT_MISMATCHED);
}

/* eval() : Evaluate the expression - "return functor(future)" that may have a void return type */

template <typename Functor, typename T>
typename std::enable_if<ret_type_is_void<Functor>::value && arg_count_is_zero<Functor>::value,
Value<RetType<Functor>>>::type
eval(Functor functor, QFuture<T> future) {
    Q_UNUSED(future);
    functor();
    return Value<void>();
}

template <typename Functor, typename T>
typename std::enable_if<ret_type_is_void<Functor>::value && !arg_count_is_zero<Functor>::value,
Value<RetType<Functor>>>::type
eval(Functor functor, QFuture<T> future) {
    // callIgnoreReturn() is designed to reduce the no. of annoying compiler error messages.
    callIgnoreReturn(functor, future);
    return Value<void>();
}

template <typename Functor, typename T>
typename std::enable_if<!ret_type_is_void<Functor>::value && arg_count_is_zero<Functor>::value,
Value<RetType<Functor>>>::type
eval(Functor functor, QFuture<T> future) {
    Q_UNUSED(future);
    return functor();
}

template <typename Functor, typename T>
typename std::enable_if<!ret_type_is_void<Functor>::value && !arg_count_is_zero<Functor>::value,
Value<RetType<Functor>>>::type
eval(Functor functor, QFuture<T> future) {
    return call(functor, future);
}

/// Create a DeferredFuture that will execute the callback functions when observed future finished
/** DeferredType - The template type of the DeferredType
 *  RetType - The return type of QFuture
 *
 * DeferredType and RetType can be different.
 * e.g DeferredFuture<int> = Value<QFuture<int>>
 */
template <typename DeferredType, typename RetType, typename T, typename Completed, typename Canceled>
static QFuture<DeferredType> execute(QFuture<T> future, QObject* contextObject, Completed onCompleted, Canceled onCanceled) {

    auto defer = DeferredFuture<DeferredType>::create();

    watch(future,
          contextObject,
          contextObject,[=]() {
        try {
            Value<RetType> value = eval(onCompleted, future);
            defer->complete(value);
        } catch (QException& e) {
            defer->reportException(e);
            defer->cancel();
        } catch (...) {
            defer->reportException(QUnhandledException());
            defer->cancel();
        }
    }, [=]() {
        onCanceled();
        defer->cancel();
    });

    if (contextObject) {
        defer->cancel(contextObject, &QObject::destroyed);
    }

    return defer->future();
}

} // End of Private Namespace

/* Start of AsyncFuture Namespace */

template <typename T>
class Deferred;

template <typename T>
class Observable {
protected:
    QFuture<T> m_future;

public:

    Observable() {

    }

    Observable(QFuture<T> future) {
        m_future = future;
    }

    QFuture<T> future() const {
        return m_future;
    }

    template <typename Completed>
    typename std::enable_if< !Private::future_traits<typename Private::function_traits<Completed>::result_type>::is_future,
    Observable<typename Private::function_traits<Completed>::result_type>
    >::type
    context(QObject* contextObject, Completed functor)  {
        /* functor return non-QFuture type */

        ASYNC_FUTURE_CALLBACK_STATIC_ASSERT("context(callback): ", Completed);

        return _context<typename Private::function_traits<Completed>::result_type,
                       typename Private::function_traits<Completed>::result_type
                >(contextObject, functor);
    }

    template <typename Completed>
    typename std::enable_if< Private::future_traits<typename Private::function_traits<Completed>::result_type>::is_future,
    Observable<typename Private::future_traits<typename Private::function_traits<Completed>::result_type>::arg_type>
    >::type
    context(QObject* contextObject, Completed functor)  {
        /* functor returns a QFuture */

        ASYNC_FUTURE_CALLBACK_STATIC_ASSERT("context(callback): ", Completed);

        return _context<typename Private::future_traits<typename Private::function_traits<Completed>::result_type>::arg_type,
                       typename Private::function_traits<Completed>::result_type
                >(contextObject, functor);
    }

    /* subscribe function */

    template <typename Completed, typename Canceled>
    typename std::enable_if<!Private::ret_type_is_future<Completed>::value,
    Observable<typename Private::RetType<Completed>>
    >::type
    subscribe(Completed onCompleted,
              Canceled onCanceled) {
        /* For functor return a regular value */

        ASYNC_FUTURE_CALLBACK_STATIC_ASSERT("subscribe(callback): ", Completed);

        return _subscribe<typename Private::function_traits<Completed>::result_type,
                         typename Private::function_traits<Completed>::result_type
                >(onCompleted, onCanceled);
    }

    template <typename Completed>
    typename std::enable_if<!Private::ret_type_is_future<Completed>::value,
    Observable<typename Private::RetType<Completed>>
    >::type
    subscribe(Completed onCompleted) {
        /* For functor return a regular value and onCanceled is missed */

        ASYNC_FUTURE_CALLBACK_STATIC_ASSERT("subscribe(callback): ", Completed);

        return _subscribe<typename Private::function_traits<Completed>::result_type,
                         typename Private::function_traits<Completed>::result_type
                >(onCompleted, [](){});
    }

    template <typename Completed, typename Canceled>
    typename std::enable_if<Private::ret_type_is_future<Completed>::value,
    Observable<typename Private::function_traits<Completed>::future_arg_type>
    >::type
    subscribe(Completed onCompleted,
              Canceled onCanceled) {
        /* onCompleted returns a QFuture */

        ASYNC_FUTURE_CALLBACK_STATIC_ASSERT("subscribe(callback): ", Completed);

        return _subscribe<typename Private::future_traits<typename Private::function_traits<Completed>::result_type>::arg_type,
                         typename Private::function_traits<Completed>::result_type
                >(onCompleted, onCanceled);
    }

    template <typename Completed>
    typename std::enable_if<Private::ret_type_is_future<Completed>::value,
    Observable<typename Private::function_traits<Completed>::future_arg_type>
    >::type
    subscribe(Completed onCompleted) {
        /* onCompleted returns a QFuture and no onCanceled given */

        ASYNC_FUTURE_CALLBACK_STATIC_ASSERT("subscribe(callback): ", Completed);

        return _subscribe<typename Private::future_traits<typename Private::function_traits<Completed>::result_type>::arg_type,
                         typename Private::function_traits<Completed>::result_type
                >(onCompleted, [](){});
    }

    /* end of subscribe function */

    template <typename Functor>
    typename std::enable_if<std::is_same<typename Private::RetType<Functor>, void>::value, void>::type
    onProgress(Functor functor) {
        onProgress([=]() mutable {
            functor();
            return true;
        });
    }

    template <typename Functor>
    typename std::enable_if<std::is_same<typename Private::RetType<Functor>,bool>::value, void>::type
    onProgress(Functor onProgressArg) {
        QFutureWatcher<T> *watcher = new QFutureWatcher<T>();

        auto wrapper = [=]() mutable {

            if (!onProgressArg()) {
                watcher->disconnect();
                watcher->deleteLater();
            }
        };

        QObject::connect(watcher, &QFutureWatcher<T>::finished,
                         [=]() {
            watcher->disconnect();
            watcher->deleteLater();
        });

        QObject::connect(watcher, &QFutureWatcher<T>::canceled,
                         [=]() {
            watcher->disconnect();
            watcher->deleteLater();
        });

        QObject::connect(watcher, &QFutureWatcher<T>::progressValueChanged, wrapper);

        QObject::connect(watcher, &QFutureWatcher<T>::progressRangeChanged, wrapper);

        if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
            watcher->moveToThread(QCoreApplication::instance()->thread());
        }

        watcher->setFuture(m_future);
    }


    void onCompleted(std::function<void()> func) {
        subscribe(func, []() {});
    }

    void onCanceled(std::function<void()> func) {
        subscribe([]() {}, func);
    }

    void onFinished(std::function<void()> func) {
        auto runOnMainThread = [=]() {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)

        QObject tmp;
        QObject::connect(&tmp, &QObject::destroyed, QCoreApplication::instance(), func, Qt::QueuedConnection);
#else
        QMetaObject::invokeMethod(QCoreApplication::instance(), func, Qt::QueuedConnection);
#endif
        };

        subscribe(runOnMainThread, runOnMainThread);
    }

    template <typename ANY>
    void onCanceled(Deferred<ANY> object) {
        subscribe([]() {}, [=]() {
            auto o = object;
            o.cancel();
        });
    }

    template <typename ANY>
    void onCanceled(QFuture<ANY> future) {
        subscribe([]() {}, [=]() {
            auto f = future;
            f.cancel();
        });
    }

private:
    template <typename ObservableType, typename RetType, typename Completed>
    Observable<ObservableType> _context(QObject* contextObject, Completed functor)  {

        auto future = Private::execute<ObservableType, RetType>(m_future,
                                                               contextObject,
                                                               functor,
                                                               [](){});

        return Observable<ObservableType>(future);
    }

    template <typename ObservableType, typename RetType, typename Completed, typename Canceled>
    Observable<ObservableType> _subscribe(Completed onCompleted, Canceled onCanceled) {       

        auto future = Private::execute<ObservableType, RetType>(m_future,
                                                               QCoreApplication::instance(),
                                                               onCompleted,
                                                               onCanceled);

        return Observable<ObservableType>(future);
    }

};

template <typename T>
class Deferred : public Observable<T> {

public:
    Deferred() : Observable<T>(),
              deferredFuture(Private::DeferredFuture<T>::create())  {
        this->m_future = deferredFuture->future();
    }

    void complete(QFuture<QFuture<T>> future) {
        deferredFuture->complete(future);
    }

    void complete(QFuture<T> future) {
        deferredFuture->complete(future);
    }

    void complete(T value)
    {
        deferredFuture->complete(value);
    }

    void complete(QList<T> value) {
        deferredFuture->complete(value);
    }

    template <typename ANY>
    void cancel(QFuture<ANY> future) {
        deferredFuture->cancel(future);
    }

    void cancel() {
        deferredFuture->cancel();
    }

    template <typename ANY>
    void track(QFuture<ANY> future) {
        deferredFuture->track(future);
    }

    void setProgressValue(int value) {
        deferredFuture->setProgressValue(value);
    }

    void setProgressRange(int minimum, int maximum) {
        deferredFuture->setProgressRange(minimum, maximum);
    }

    void reportStarted() {
        deferredFuture->reportStarted();
    }

protected:
    QSharedPointer<Private::DeferredFuture<T> > deferredFuture;
};

template<>
class Deferred<void> : public Observable<void> {

public:
    Deferred() : Observable<void>(),
              deferredFuture(Private::DeferredFuture<void>::create())  {
        this->m_future = deferredFuture->future();
    }

    template <typename ANY>
    void complete(QFuture<QFuture<ANY>> future) {
        Q_UNUSED(future);

        static_assert(Private::False<ANY>::value, "Deferred<void>::complete(QFuture<QFuture<ANY>>) is not supported");
    }

    void complete(QFuture<void> future) {
        deferredFuture->complete(future);
    }

    void complete() {
        deferredFuture->complete();
    }

    template <typename ANY>
    void cancel(QFuture<ANY> future) {
        deferredFuture->cancel(future);
    }

    void cancel() {
        deferredFuture->cancel();
    }

    template <typename ANY>
    void track(QFuture<ANY> future) {
        deferredFuture->track(future);
    }

    void reportStarted() {
        deferredFuture->reportStarted();
    }

protected:
    QSharedPointer<Private::DeferredFuture<void> > deferredFuture;
};

typedef enum {
    FailFast,
    AllSettled
} CombinatorMode;

class Combinator : public Observable<void> {
private:
    QSharedPointer<Private::CombinedFuture> combinedFuture;

public:
    inline Combinator(CombinatorMode mode = FailFast) : Observable<void>() {
        combinedFuture = Private::CombinedFuture::create(mode == AllSettled);
        m_future = combinedFuture->future();
    }

    inline ~Combinator() {
        if (!combinedFuture.isNull() && combinedFuture->future().progressMaximum() == 0) {
            // No future added
            combinedFuture->deleteLater();
        }
    }

    template <typename T>
    Combinator& combine(QFuture<T> future) {
        combinedFuture->addFuture(future);
        return *this;
    }

    template <typename T>
    Combinator& operator<<(QFuture<T> future) {
        combinedFuture->addFuture(future);
        return *this;
    }

    template <typename T>
    Combinator& operator<<(Deferred<T> deferred) {
        combinedFuture->addFuture(deferred.future());
        return *this;
    }
};

template <typename T>
static Observable<T> observe(QFuture<QFuture<T>> future) {
    Deferred<T> defer;
    defer.complete(future);
    return Observable<T>(defer.future());
}

template <typename T>
static Observable<T> observe(QFuture<T> future) {
    return Observable<T>(future);
}

template <typename Member>
auto observe(QObject* object, Member pointToMemberFunction)
-> Observable< typename Private::signal_traits<Member>::result_type> {

    typedef typename Private::signal_traits<Member>::result_type RetType;

    auto defer = Private::DeferredFuture<RetType>::create();

    auto proxy = new Private::Proxy<RetType>(nullptr);

    QObject::connect(object, &QObject::destroyed, proxy, [=]() {
       defer->cancel();
       delete proxy;
    });

    proxy->bind(object, pointToMemberFunction);
    proxy->callback = [=](Private::Value<RetType> value) {
        defer->complete(value);
        delete proxy;
    };

    Observable< typename Private::signal_traits<Member>::result_type> observer(defer->future());
    return observer;
}

inline Observable<QVariant> observe(QObject *object,QString signal)  {

    auto defer = Private::DeferredFuture<QVariant>::create();

    auto future = defer->future();

    auto proxy = new Private::Proxy2(nullptr);

    QObject::connect(object, &QObject::destroyed, proxy, [=]() {
       defer->cancel();
       delete proxy;
    });

    if (proxy->bind(object, signal)) {
        proxy->callback = [=](QVariant value) {
            defer->complete(value);
            delete proxy;
        };
    } else {
        defer->cancel();
        delete proxy;
    }

    Observable<QVariant> observer(future);
    return observer;
}

template <typename T>
auto deferred() -> Deferred<T> {
    return Deferred<T>();
}

inline Combinator combine(CombinatorMode mode = FailFast) {
    return Combinator(mode);
}

}
