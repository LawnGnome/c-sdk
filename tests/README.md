# Unit Testing the C-Agent

### Description

The C-Agent uses the cmocka framework for unit-testing.  This README offers guidance on setting up one's development 
environment to install the framework and run the unit tests.

### Requirements

* cmocka 1.1.1, available at ~/vendor/cmocka.

### Getting started

Build and install cmocka.  From the top-level of this repository:

```
make vendor
```

To run unit-tests, from the top of the C-Agent repository,

```
make all
make run_tests
```
