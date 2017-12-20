# C Agent Release Checklist

1. Make a release commit

- Update [VERSION](https://source.datanerd.us/c-agent/c-agent/blob/master/VERSION) file to the current release
- Update [CHANGELOG.md](https://source.datanerd.us/c-agent/c-agent/blob/master/CHANGELOG.md)

2. Kick off Jenkins

- Start the process by building [c-agent-cut-a-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-cut-a-release/build?delay=0sec) with the `VERSION` number found in the VERSION file.  The downstream jobs will perform the following actions

    - @TODO: Sanity check the version given as the input parameter versus what is found in the VERSION file of the repository
    - [Build](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-build/)
    - Test ([cmocka](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-cmocka/), [axiom](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-axiom/), [axiom valgrind](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-axiom-valgrind/), [daemon integration](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-daemon-tests/))
    - [Create a release branch](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-branch/) on the master repository with a branch name of R`VERSION`.  Example: R1.0.0
    - [Package the C-agent](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tarball/) as a tarball
    - @TODO Upload tarball to testing server (In the future the multiJob will probably have a parameter to select `testing` or `production`)
    - @TODO Download tarball and confirm it works with `test_app`

3. Are all the [Jenkins CI jobs](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-cut-a-release/) blue?

4. [For production only] Send an email to `agent-releases@newrelic.com` with the release notes

5. [For production only] Are there new docs staged?  If so tell the `#documentation` `@hero` to release the docs!
