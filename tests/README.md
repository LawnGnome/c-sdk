# Unit Testing the C-Agent

### Description

The C-Agent uses the cmocka framework for unit-testing.  This README offers guidance on setting up one's development 
environment to install the framework and run the unit tests.

### Requirements

* cmocka 1.1.1, available at ~/vendor/cmocka-1.1.1

### Getting started

Build and install cmocka

```
cd vendor/cmocka-1.1.1
mkdir cmocka-1.1.1/build
cd cmocka-1.1.1/build
cmake ..
make
sudo make install
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

To run unit-tests, from the top of the C-Agent repository,

```
make all
make run_tests
```
