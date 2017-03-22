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

QFuture<QImage> result = (combine() << f1 << f2).subscribe([=](){
    // Read an image but do not return before timeout
    return f1.result();
}).future();

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

**4. Use QFuture like a Promise object**

Create a QFuture and Complete / cancel it by yourself.

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

QCOMPARE(d.future().isFinished(), false);
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

This function creates an Observable&lt;ARG&gt; object which contains a future to represent the result of the signal. You could obtain the future by the future() method. And observe the result by subscribe() / context() methods

The ARG type is equal to the first parameter of the signal. If the signal does not contain any argument, ARG will be void. In case it has more than one argument, the rest will be ignored.

```c++
QFuture<void> f1 = observe(timer, &QTimer::timeout).future();
QFuture<bool> f2 = observe(button, &QAbstractButton::toggled).future();
```

See [Observable`<T>`](#observablet)

AsyncFuture::observe(QFuture&lt;T&gt; future)
-------------

This function creates an Observable<T> object which provides an interface for observing the input future. See [Observable`<T>`](#observablet)

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


AsyncFuture::combine(CombinatorMode mode = FailFast)
------------

This function creates a Combinator object (inherit `Observable<void>`) for combining multiple future objects with different type into a single future.

For example:

```c++

QFuture<QImage> f1 = QtConcurrent::run(readImage, QString("image.jpg"));
QFuture<void> f2 = observe(timer, &QTimer::timeout).future();

QFuture<QImage> result = (combine(AllSettled) << f1 << f2).subscribe([=](){
    // Read an image but do not return before timeout
    return f1.result();
}).future();

```

Once all the observed futures finished, the contained future will be finished too.  And it will be cancelled immediately if any one of the observed future is cancelled in fail fast mode. In case you wish the cancellation take place after all the futures finished, you should set mode to `AllSettled`.


AsyncFuture::deferred&lt;T&gt;()
----------

The deferred() function return a Deferred object that allows you to set QFuture completed/cancelled manually. 

```c++
auto d = deferred<bool>();

observe(d.future()).subscribe([]() {
    qDebug() << "onCompleted";
}, []() {
    qDebug() << "onCancel";
});

d.complete(true); // or d.cancel();
```

See [`Deferred<T>`](#deferredt)

Observable&lt;T&gt;
------------

Obsevable<T> is a chainable utility for observing a QFuture object. It is created by the observe() function. It can register multiple callbacks to be triggered in different situations. And that will create a new Observable<T> / QFuture object to represent the result of the callback function. It may even call QtConcurrent::run() within the callback function to create a thread. Therefore, it could create a more complex/flexible workflow.

**QFuture&lt;T&gt; future()**

Obtain the observing QFuture object to represent the result of this Observable object

**Observable&lt;R&gt; context(QObject&#42; contextObject, Completed onCompleted)**

Add a callback function that listens to the finished signal from the observing QFuture object. The callback won't be triggered if the future is cancelled.

The callback is invoked in the thread of the context object, In case the context object is destroyed before the finished signal, the callback function won't be triggered and the returned Observable object will cancel its future.

The return value is an `Observable<R>` object where R is the return type of the onCompleted callback.

```c++

auto validator = [](QImage input) -> bool {
   /* A dummy function. Return true for any case. */
   return true;
};

QFuture<QImage> reading = QtConcurrent::run(readImage, QString("image.jpg"));

QFuture<bool> validating = observe(reading).context(contextObject, validator).future();
```

In the above example, the result of `validating` is supposed to be true. However, if the `contextObject` is destroyed before `reading` future finished, it will be cancelled and the result will become undefined.


**Observable&lt;T&gt; subscribe(Completed onCompleted, Canceled onCanceled)**

Completed Callback Funcion
---------------

In subscribe() / context(), you may pass a function with zero or one argument as the onCompleted callback. If you give it an argument, the type must be either of T or QFuture<T>. That would obtain the result or the observed future itself.

```c++
QFuture<QImage> reading = QtConcurrent::run(readImage, QString("image.jpg"));

observe(reading).subscribe([]() {
});

observe(reading).subscribe([](QImage result) {
});

observe(reading).subscribe([](QFuture<QImage> future) {
  // In case you need to get future.results
});
```

The return type can be none or any kind of value. That would determine what type of `Observable<R>` generated by context()/subscribe().

In case, you return a QFuture object. Then the new `Observable<R>` object will be deferred to complete/cancel until your future object is resolved. Therefore, you could run QtConcurrent::run within your callback function to make a more complex/flexible multiple threading programming models.

```c++

QFuture<int> f1 = QtConcurrent::mapped(input, mapFunc);

QFuture<int> f2 = observe(f1).context(contextObject, [=](QFuture<int> future) {
    // You may use QFuture as the input argument of your callback function
    // It will be set to the observed future object. So that you may obtain
    // the value of results()

    qDebug() << future.results();

    // Return another thread is possible.
    return QtConcurrent::run(reducerFunc, future.results());
}).future();

// f2 is constructed before the QtConcurrent::run statement
// But its value is equal to the result of reducerFunc

```


Deferred&lt;T&gt;
-----------

The `deferred<T>`() function return a Deferred<T> object that allows you to manipulate a QFuture manually. The future() function return a forever running QFuture<T> unless you have called Deferred.complete() / Deferred.cancel() manually, or the Deferred object is destroyed without observed any future.

The usage of complete/cancel with a Deferred object is pretty similar to the resolve/reject in a Promise object. You could complete a future by calling complete with a result value. If you give it another future, then it will observe the input future and change status once that is finished.

**complete(T) / complete()**

**complete(QFuture<T>)**

**cancel()**

**cancel(QFuture<ANY>)**











