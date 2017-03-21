## AsyncFuture - Use QFuture like a Promise object
[![Build Status](https://travis-ci.org/benlau/asyncfuture.svg?branch=master)](https://travis-ci.org/benlau/asyncfuture)

QFuture is usually used together with QtConcurrent to represent the result of an asynchronous computation. It is a powerful component for multi-thread programming. But its usage is limited to the result of threads. And QtConcurrent only provides a MapReduce model that may not fit your use cases. Moreover, it doesn't work with the asynchronous signal emitted by QObject. And it is a bit trouble to setup the listener function via QFutureWatcher.

AsyncFuture is designed to enhance the function to offer a better way to use it for asynchronous programming. It provides a Promise object like interface. This project is inspired by AsynQt and RxCpp.

Remarks: You may use this project together with [QuickFuture](https://github.com/benlau/quickfuture) for QML programming.

Features
========

**1. Convert a signal from QObject into a QFuture object**

```c++

#include "asyncfuture.h"
using namespace AsyncFuture;

// Convert a signal from QObject into a QFuture object

QFuture<void> future = observe(timer,
                               &QTimer::timeout).future();

/* Listen from the future without using QFutureWatcher<T>*/
observe(future).subscribe([]() {
    // onCompleted. It is invoked when the observed future is finished successfully
    qDebug() << "onCompleted";
},[]() {
    // onCanceled
    qDebug() << "onCancel";
});

/* It is chainable. Listen from a timeout signal only once */
observe(timer, &QTimer::timeout).subscribe([=]() { /*…*/ });
```

**2. Combine multiple futures with different type into a single future object**

```c++
/* Combine multiple futures with different type into a single future */

QFuture<QImage> f1 = QtConcurrent::run(readImage, QString("image.jpg"));

QFuture<void> f2 = observe(timer, &QTimer::timeout).future();

(combine() << f1 << f2).subscribe([](QVariantList result){
    // Read an image
    // result[0] = QImage
    qDebug() << result;
});
```

**3. Advanced multi-threading model**

```c++
/* Start a thread and process its result in main thread */

QFuture<QImage> reading = QtConcurrent::run(readImage, QString("image.jpg"));

QFuture<bool> validating = observe(reading).context(contextObject, validator).future();

    // Read image by a thread, when it is ready, run the validator function
    // in the thread of the contextObject(e.g main thread)
    // And it return another QFuture to represent the final result.

/* Start a thread and process its result in main thread, then start another thread. */

QFuture<int> f1 = QtConcurrent::mapped(input, mapFunc);

QFuture<int> f2 = observe(f1).context(contextObject, [=](QFuture<int> future) {
    // You may use QFuture as the input argument of your callback function
    // It will be set to the observed future object. So that you may obtain
    // the value of results()

    qDebug() << future.results();

    // Return another QFuture is possible.
    return QtConcurrent::run(reducerFunc, future.results());
}).future();

// f2 is constructed before the QtConcurrent::run statement
// But its value is equal to the result of reducerFunc

```

**4. Promise like interface**

Complete / cancel a future on your own choice

```c++
// Complete / cancel a future on your own choice
auto d = deferred<bool>();

observe(d.future()).subscribe([]() {
    qDebug() << "onCompleted";
}, []() {
    qDebug() << "onCancel";
});

d.complete(true); // or d.cancel();

QCOMPARE(d.future().isFinished(), true);
QCOMPARE(d.future().isCanceled(), false);

```

Complete / cancel a future according to another future object.


```c++
// Complete / cancel a future according to another future object.

auto d = deferred<void>();

d.complete(QtConcurrent::run(timeout));

QCOMPARE(d.future().isFinished(), true);
QCOMPARE(d.future().isCanceled(), false);

```

Read a file. If timeout, cancel it.

```c++

auto timeout = observe(timer, &QTimer::timeout).future();

auto defer = deferred<QString>();

defer.complete(QtConcurrent::run(readFileworker, fileName));
defer.cancel(timeout);

return defer.future();
```

The deferred<T>() function return a Deferred<T> object that allows you to manipulate a QFuture manually. The future() function return a forever running QFuture<T> unless you have called Deferred.complete() / Deferred.cancel() manually, or the Deferred object is destroyed without observed any future.

The usage of complete/cancel with a Deferred object is pretty similar to the resolve/reject in a Promise object. You could complete a future by calling complete with a result value. If you give it another future, then it will observe the input future and change status once that is finished.

More examples are available at : [asyncfuture/example.cpp at master · benlau/asyncfuture](https://github.com/benlau/asyncfuture/blob/master/tests/asyncfutureunittests/example.cpp)

Installation
=============

AsyncFuture is a single header library. You could just download the `asyncfuture.h` in your source tree or install it via qpm

    qpm install async.future.pri

or

    wget https://raw.githubusercontent.com/benlau/asyncfuture/master/asyncfuture.h

API
===

Still under construction


AsyncFuture::observe(QObject* object, PointerToMemberFunc signal)
-------------------

This function creates an Observable&lt;ARG&gt; object which contains a future to represent the result of the signal.You could obtain the future by the future() method. And observe the result of the future by subscribe() / context() methods

The ARG type is equal to the first parameter of the signal. If the signal does not contain any argument, ARG will be void. In case it has more than one argument, the rest will be ignored.

```c++
QFuture<void> f1 = observe(timer, &QTimer::timeout).future();
QFuture<bool> f2 = observe(button, &QAbstractButton::toggled).future();
```

AsyncFuture::observe(QFuture&lt;T&gt; future)
-------------

This function creates an Observable<T> object which provides an interface for observing the input future. See [Observable<T>](#observable)

```c++
// Convert a signal from QObject into QFuture
QFuture<bool> future = observe(button, &QAbstractButton::toggled).future();


// Listen from the future without using QFutureWatcher<T>
observe(future).subscribe([](bool toggled) {
    // onCompleted. It is invoked when the observed future is finished successfully
    qDebug() << "onCompleted";
},[]() {
    // onCanceled
    qDebug() << "onCancel";
});

observe(future).context(context, [](bool toggled) {
    // simialr to subscribe, but this function will not be invoked if context object
    // is destroyed.
});

```


AsyncFuture::combine(bool settleAllMode)
------------



AsyncFuture::deferred&lt;T&gt;()
----------

Observable&lt;T&gt;
------------

**context**

**subscribe**

Deferred<T>
-----------

**complete(T) / complete()**

**complete(QFuture<T>)**

**cancel()**

**cancel(QFuture<ANY>)**










