## AsyncFuture - Use QFuture like a Promise object
[![Build Status](https://travis-ci.org/benlau/asyncfuture.svg?branch=master)](https://travis-ci.org/benlau/asyncfuture)
[![Build status](https://ci.appveyor.com/api/projects/status/5cndw1uu5ay960c4?svg=true)](https://ci.appveyor.com/project/benlau/asyncfuture)


QFuture is used together with QtConcurrent to represent the result of an asynchronous computation. It is a powerful component for multi-thread programming. But its usage is limited to the result of threads, it doesn't work with the asynchronous signal emitted by QObject. And it is a bit trouble to setup the listener function via QFutureWatcher.

AsyncFuture is designed to enhance the function to offer a better way to use it for asynchronous programming. It provides a Promise object like interface. This project is inspired by AsynQt and RxCpp.

Remarks: You may use this project together with [QuickFuture](https://github.com/benlau/quickfuture) for QML programming.

Reference Articles
------------------

1. [Multithreading Programming with Future & Promise – E-Fever – Medium](https://medium.com/e-fever/multithreading-programming-with-future-promise-2d35e13b9404)

Features
========

 1. Convert a signal from QObject into a QFuture object
 2. Combine multiple futures with different type into a single future object
 3. Use QFuture like a Promise object
 4. Chainable Callback - Advanced multi-threading programming model

**1. Convert a signal from QObject into a QFuture object**

```c++

#include "asyncfuture.h"
using namespace AsyncFuture;

// Convert a signal from QObject into a QFuture object

QFuture<void> future = observe(timer, &QTimer::timeout).future();

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

**3. Use QFuture like a Promise object**

Create a QFuture then complete / cancel it by yourself.

```c++
// Complete / cancel a future on your own choice
auto d = deferred<bool>();

d.subscribe([]() {
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

**4. Chainable Callback - Advanced multi-threading programming model**

Futures can be chained into a sequence of process. And represented by a single future object.

```c++
/* Start a thread and process its result in main thread */

QFuture<QImage> readImage(const QString& file) {

    auto readImageWorker = [=]() {
        QImage image;
        image.read(file);
        return image;
    };
    
    auto updateCache = [&](QImage image) {
        m_cache[file] = image;
        return image;
    };

    QFuture<QImage> reading = QtConcurrent::run(readImageWorker));
    return observe(reading).subscribe(updateCache).future();   
}

// Read image by a thread, when it is ready, run the updateCache function
// in the main thread.
// And it return another QFuture to represent the final result.

```

```c++
/* Start a thread and process its result in main thread, then start another thread. */

QFuture<int> f1 = QtConcurrent::mapped(input, mapFunc);

QFuture<int> f2 = observe(f1).subscribe([=](QFuture<int> future) {
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


More examples are available at : [asyncfuture/example.cpp at master · benlau/asyncfuture](https://github.com/benlau/asyncfuture/blob/master/tests/asyncfutureunittests/example.cpp)

Installation
=============

AsyncFuture is a single header library. You could just download the `asyncfuture.h` in your source tree or install it via qpm

    qpm install async.future.pri

or

    wget https://raw.githubusercontent.com/benlau/asyncfuture/master/asyncfuture.h

API
===

AsyncFuture::observe(QObject* object, PointerToMemberFunc signal)
-------------------

This function creates an Observable&lt;ARG&gt; object which contains a future to represent the result of the signal. You could obtain the future by the future() method. And observe the result by subscribe() / context() methods

The ARG type is equal to the first parameter of the signal. If the signal does not contain any argument, ARG will be void. In case it has more than one argument, the rest will be ignored.

```c++
QFuture<void> f1 = observe(timer, &QTimer::timeout).future();
QFuture<bool> f2 = observe(button, &QAbstractButton::toggled).future();
```

See [Observable`<T>`](#observablet)

AsyncFuture::observe(object, SIGNAL(signal))
----------------------

This function creates an `Observable<QVariant>` object which contains a future to represent the result of the signal. You could obtain the future by the future() method. And observe the result by subscribe() / context() methods. The result of the future is equal to the first parameter of the signal.

```c++
QFuture<QVariant> future = observe(timer, SIGNAL(timeout()).future();
```

See [Observable`<T>`](#observablet)

AsyncFuture::observe(QFuture&lt;T&gt; future)
-------------

This function creates an Observable&lt;T&gt; object which provides an interface for observing the input future. See [Observable`<T>`](#observablet)

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

Since v3.6, you may assign a deferred object to Combinator directly.

Example 

```
QFuture<QImage> f1 = QtConcurrent::run(readImage, QString("image.jpg"));
auto defer = deferred<void>();

QFuture<QImage> result = (combine(AllSettled) << f1 << defer).subscribe([=](){
    // Read an image but do not return before the deferred is completed
    return f1.result();
}).future();
```

AsyncFuture::deferred&lt;T&gt;()
----------

The deferred() function return a Deferred object that allows you to set QFuture completed/cancelled manually. 

```c++
auto d = deferred<bool>();

d.subscribe([]() {
    qDebug() << "onCompleted";
}, []() {
    qDebug() << "onCancel";
});

d.complete(true); // or d.cancel();
```

See [`Deferred<T>`](#deferredt)

![AsyncFuture Class Diagram](https://raw.githubusercontent.com/benlau/junkcode/master/docs/AsyncFuture%20Class%20Diagram.png)

Observable&lt;T&gt;
------------

Observable&lt;T&gt; is a chainable utility class for observing a QFuture object. It is created by the observe() function. It can register multiple callbacks to be triggered in different situations. And that will create a new Observable&lt;T&gt; / QFuture object to represent the result of the callback function. It may even call QtConcurrent::run() within the callback function to run the funciton in another thread. Therefore, it could create a more complex/flexible workflow.

```
QFuture<int> future

Observable<int> observable1 = AsyncFuture::observe(future); 
// or
auto observable2 = AsyncFuture::observe(future); 
```

**QFuture&lt;T&gt; Observable&lt;T&gt;::future()**

Obtain the QFuture object to represent the result.

**Observable&lt;T&gt; Observable&lt;T&gt;::subscribe(Completed onCompleted, Canceled onCanceled)**

    Observable<T> Observable<T>::subscribe(Completed onCompleted);
    Observable<T> Observable<T>::subscribe(Completed onCompleted, Canceled onCanceled);

Register a onCompleted and/or onCanceled callback to the observed QFuture object. Unlike the context() function, the callbacks will be triggered as long as the current thread exists. The return value is an Observable<R> object where R is the return type of the onCompleted callback.

Remarks: After v0.3.2, the callback will be executed in main thread.

```c++
QFuture<bool> future = observe(button, &QAbstractButton::toggled).future();

// Listen from the future without using QFutureWatcher<T>
observe(future).subscribe([](bool toggled) {
    // onCompleted. It is invoked when the observed future is finished successfully
    qDebug() << "onCompleted";
},[]() {
    // onCanceled
    qDebug() << "onCancel";
});

```

**Observable&lt;R&gt; Observable&lt;T&gt;::context(QObject&#42; contextObject, Completed onCompleted)**

*This API is for advanced users only*

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

**void Observable&lt;T&gt;::onProgress(Functor callback)**

Listens the `progressValueChanged` and `progressRangeChanged` signal from the observed future then trigger the callback. The callback function may return nothing or a boolean value. If the boolean value is false, it will remove the listener such that the callback may not trigger anymore.

Example

```c++
QFuture<int> future = QtConcurrent::mapped(input, workerFunction);

AsyncFuture::observe(future).onProgress([=]() -> bool {
    qDebug() << future.progressValue();
    return true;
});

// or

AsyncFuture::observe(future).onProgress([=]() -> void {
    qDebug() << future.progressValue();
});

```

Added since v0.3.6.4

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

In case, you return a QFuture object. Then the new `Observable<R>` object will be deferred to complete/cancel until your future object is resolved. Therefore, you could run QtConcurrent::run within your callback function to make a more complex/flexible multi-threading programming models.

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

Callback Chain Cancelation
----

A chain can be canceled by returning a canceled QFuture.

Example:

```c++
auto f2 = observe(f1).subscribe([=]() {
  auto defer = Deferred<void>();
  defer.cancel();
  return defer.future();
}).future();

observe(f2).subscribe([=]() {
  // it won't be executed.
});
```


Deferred&lt;T&gt;
-----------

The `deferred<T>()` function return a Deferred<T> object that allows you to manipulate a QFuture manually. The future() function return a running QFuture<T>. You have to call Deferred.complete() / Deferred.cancel() to trigger the status changes.

The usage of complete/cancel in a Deferred object is pretty similar to the resolve/reject in a Promise object. You could complete a future by calling complete with a result value. If you give it another future, then it will observe the input future and change status once that is finished.

**Auto Cancellation**

The `Deferred<T>` object is an explicitly shared class. You may own multiple copies and they are pointed to the same piece of shared data. In case, all of the instances are destroyed, it will cancel its future automatically.

But there has an exception if you have even called Deferred.complete(`QFuture<T>`) / Deferred.cancel(`QFuture<ANY>`) then it won't cancel its future due to destruction. That will leave to the observed future to determine the final state.

```c++
  QFuture<void> future;
  {

    auto defer = deferred<void>();
    future = defer.future();
  }
  QCOMPARE(future.isCanceled(), true); // Due to auto cancellation
```

```c++
  QFuture<void> future;
  {

    auto defer = deferred<void>();
    future = defer.future();
    defer.complete(QtConcurrent::run(worker));
  }
  QCOMPARE(future.isCanceled(), false);
```

**Deferred&lt;T&gt;::complete(T) / Deferred&lt;T&gt;::complete()**

Complete this future object with the given arguments

**Deferred&lt;T&gt;::complete(QList&lt;T&gt;)**

Complete the future object with a list of result. User may obtain all the value by QFuture::results().

**Deferred&lt;T&gt;::complete(QFuture&lt;T&gt;)**

This future object is deferred to complete/cancel. It will track the state from the input future. If the input future is completed, then it will be completed too. That is same for cancel.


**Deferred&lt;T&gt;::cancel()**

Cancel the future object

**Deferred&lt;T&gt;::cancel(QFuture<ANY>)**

This future object is deferred to cancel according to the input future. Once it is completed, this future will be cancelled. However, if the input future is cancelled. Then this future object will just ignore it. Unless it fulfils the auto-cancellation rule.

**Deferred&lt;T&gt;::track(QFuture target)**

Track the progress and synchronize the status of the target future object.

It will forward the signal of `started` , `resumed` , `paused` . And synchonize  the `progressValue`, `progressMinimum` and `progressMaximum` value by listening the  `progressValueChanged` signal from target future object.

Remarks: It won't complete a future even the `progressValue` has been reached the maximum value.

Added since v0.3.6

Example

```
auto defer = AsyncFuture::deferred<void>();

auto mappedFuture = QtConcurrent::mapped(data, workerFunc);

defer.track(mappedFuture);

AsyncFuture::observe(mappedFuture).subscribute([=]() mutable {
   defer.complete();
});

return defer.future(); // It is a future with progress value same as the mappedFuture, but it don't contains the result.
```


Examples
========

There has few examples of different use-cases in this source file:

[asyncfuture/example.cpp at master · benlau/asyncfuture](https://github.com/benlau/asyncfuture/blob/master/tests/asyncfutureunittests/example.cpp)



