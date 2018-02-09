# New Relic C Agent

### Description
Generic library to communicate with New Relic.

### Requirements

* cmake
* cmocka 1.1.1, (available in the `vendor` directory)
* golang 1.4+
* libpcre 8.13+

### Getting started

Clone the agent
```
git clone git@source.datanerd.us:c-agent/c-agent.git
```

Compile the agent
```
make
```

Compile and run unit tests
```
make run_tests
```

Compile and start the daemon
```
make daemon
./php_agent/bin/daemon -f --logfile stdout --loglevel debug
```

Compile and run the test app, (the `test_app` requires a running daemon to work properly -- see above)

```
make test_app
./test_app
```

Check out the data in the [PHP test account](https://staging.newrelic.com/accounts/432507/applications/)!

### What do we give customers?

```
libnewrelic
|-- bin
|   |-- newrelic-daemon
|-- examples
    |-- *
|-- libnewrelic.h
|-- libnewrelic.a
|-- GUIDE.md
|-- LICENSE.txt
```

### Dependencies 

The C Agent is dependent on code [from the php_agent repository](https://source.datanerd.us/php-agent/php_agent). This code is managed via git subtrees. This code lives in the `php_agent` folder of this repository. 

For day to day development, you don't need to be aware of this, i.e. this repo will `make` with additional steps.

When we need a fix/feature that's been added to the upstream [php_agent repository](https://source.datanerd.us/php-agent/php_agent), we use `git subtree` to pull in those changes. We've also codified this process in the

    ./tools/manage-subtree.bash
    
shell script. Use the 

    ./tools/manage-subtree.bash set_commit_hashes
    
invokation to set the commits SHAs you want to fetch, and then run 

    ./tools/manage-subtree.bash update_subtrees
    
The `manage-subtree.bash` script will also automatically update a `vendor.xml` file for each subtree dependency. These files include the relative folder name managed by `git subtree`, as well as the current commit SHA.      
