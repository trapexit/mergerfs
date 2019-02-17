# ecfd: embeddable C feature discovery

This is a simple script and set of tests to help with what would traditionally be the `configure` step of most C/C++. It is intended to be simple to be embedded into a project and only depends on the C compiler and /bin/sh.


## USAGE

First copy `ecfd` into your project. Next simply call the build script which will run the tests and output a `config.` style file to `stdout`. Compile errors will output to `stderr` which can be used to understand why they aren't working.

```
$ ./ecfd/build > include/config.h
```

## TESTS

Each test is in a single module. The build script will try to compile each module and on success will execute the program. If anything is printed to `stdout` it will be appended to the `#define` printed to the build script's `stdout`. If the compilation fails nothing will be printed. Look at the tests directory for examples.
