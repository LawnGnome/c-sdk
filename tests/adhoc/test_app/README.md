# `test_app`

The original test application when we were first building the C agent. Still a
useful tool to exercise most of the agent's functionality in one place.

## Building

`make` will build a `test_app` binary that can be run.

By default, the Makefile will look for the C agent at the project root, but you
can override this by providing the `LIBNEWRELIC_LIB` variable with the path to
the `libnewrelic.a` you would like to use. Similarly, `LIBNEWRELIC_INCLUDE` may
be used to override the include directory that will be used.

## Running

Running `./test_app` should result in a couple of transactions being sent,
provided a daemon is running beforehand.
