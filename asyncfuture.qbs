import qbs

StaticLibrary {
    name: "asyncfuture"

    Depends { name: "cpp" }

    cpp.includePaths: [
        "."
    ]

    Export {
        Depends { name: "cpp" }
        cpp.includePaths: [
            "."
        ]
    }

}
