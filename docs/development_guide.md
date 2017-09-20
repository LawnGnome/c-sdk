# C Agent Development Guide
This guide is intended to be both a beginner installation guide as well as a developer reference.

Here are the non-exhaustive capabilities of the Makefile:

|command|function|
|-------|--------|
|`make`|Build libnewrelic shared object library|
|`make axiom`|Build the axiom library|
|`make daemon`|build the daemon|
|`make test_app`|Build the test app against the libnewrelic shared object library|

## Steps to run the included test_app successfully
1. make
2. make test_app
3. Start the Daemon (Refer to the [PHP Agent Development Guide](https://source.datanerd.us/php-agent/php_agent/blob/master/docs/development_guide.md))
4. Run the compiled test app
