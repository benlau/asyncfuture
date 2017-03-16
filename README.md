## AsyncFuture - Enhance QFuture for asynchronous programming

QFuture represents the result of an asynchronous computation. It is a powerful component for multi-thread programming. But its usage is limited to the result of threads. It doesn't work with the asynchronous event/signal emitted by QObject. And it is a bit trouble to setup the listener function via QFutureWatcher. AsyncFuture is designed to enhance the function to offer a better way to use it for asynchronous programming. This project is inspired by AsynQt and RxCpp. 
