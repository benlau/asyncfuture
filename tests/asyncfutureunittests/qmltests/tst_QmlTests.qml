import QtQuick 2.0
import QtTest 1.0
import Testable 1.0

Item {
    id: window
    height: 640
    width: 480

    TestCase {
        name: "QmlTests"
        when: windowShown

        function test_dummy() {
        }
    }
}
