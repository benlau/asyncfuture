#!/bin/bash

export DISPLAY=:99.0
export QT_QPA_PLATFORM=minimal

set -v
set -e

ulimit -c unlimited 
ulimit -a

pushd tests/asyncfutureunittests
qmake asyncfutureunittests.pro
make
./asyncfutureunittests

ls
if test -f coredump; then gdb -ex "where \n ;  thread apply all bt" asyncfutureunittests coredump  ; fi
valgrind --num-callers=30 --leak-check=full --track-origins=yes --gen-suppressions=all --error-exitcode=1 --suppressions=./asyncfuture.supp ./asyncfutureunittests
popd
