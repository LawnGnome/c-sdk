# New Relic C Agent

A generic library to communicate with New Relic from any language with a C FFI
mechanism.

## Building the agent

### Requirements

* gcc or clang (any version in the last ten years is likely fine)
* cmake
* golang 1.4+
* libpcre 8.20+

Note that the unit tests require cmocka 1.1.1 or later, but this is vendored in
the `vendor` directory.

### Agent

Building the agent should be as simple as:

```sh
make
```

This will create a `libnewrelic.a` in this directory, ready to link against.

### Daemon

To build the daemon, run:

```sh
make daemon
```

This creates `php_agent/bin/daemon`. You can then start the daemon in the
foreground with:

```sh
./php_agent/bin/daemon -f --logfile stdout --loglevel debug
```

## Using the agent

API usage information can be found in [the guide](GUIDE.md).

### Headers

Note that only `include/libnewrelic.h` contains the stable, public API. Other
header files are internal to the agent, and their stability is not guaranteed.

### API reference

You can use [doxygen](http://www.doxygen.nl/) to generate API reference
documentation:

```sh
doxygen
```

This will create HTML output in the `html` directory.

## Running tests

### Unit tests

To compile and run the unit tests:

```sh
make run_tests
```

### Ad hoc tests

A number of ad hoc tests have been provided. More information can be found in
[the ad hoc test readme](tests/adhoc/README.md).

## Dependencies 

The C Agent is dependent on code
[from the php_agent repository](https://source.datanerd.us/php-agent/php_agent).
This code is managed via git subtrees. This code lives in the `php_agent`
folder of this repository. 

For day to day development, you don't need to be aware of this, i.e. this repo
will `make` with additional steps.

When we need a fix/feature that's been added to the upstream
[php_agent repository](https://source.datanerd.us/php-agent/php_agent), we use
`git subtree` to pull in those changes. We've also codified this process in the

    ./tools/manage-subtree.bash
    
shell script. Use the 

    ./tools/manage-subtree.bash set_commit_hashes
    
invokation to set the commits SHAs you want to fetch, and then run 

    ./tools/manage-subtree.bash update_subtrees
    
The `manage-subtree.bash` script will also automatically update a `vendor.xml`
file for each subtree dependency. These files include the relative folder name
managed by `git subtree`, as well as the current commit SHA.      
