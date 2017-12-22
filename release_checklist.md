# C Agent Release Checklist

1. Make a release commit.

- Ensure New Feature and Bug Fix section reflect the reality of the new release

- Update [VERSION](https://source.datanerd.us/c-agent/c-agent/blob/master/VERSION) file to the current release.

- Copy the *Master* section of  [CHANGELOG.md](https://source.datanerd.us/c-agent/c-agent/blob/master/CHANGELOG.md) to the current release version, add a new empty *Master* section to the top of the document.


2. Kick off Jenkins.

- Start the process by building [c-agent-cut-a-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-cut-a-release/build?delay=0sec) with the `VERSION` number found in the [VERSION file](https://source.datanerd.us/c-agent/c-agent/blob/master/VERSION).  The downstream jobs will perform the following actions.

    - The [c-agent-compare-version](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-compare-version/) job sanity checks the version given as the input parameter versus what is found in the VERSION file of the repository.
    - The [c-agent-release-build](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-build/) job builds the agent by compiling the source tree and creating Jenkins build artifacts.
    - The ([c-agent-release-tests-cmocka](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-cmocka/), [c-agent-release-tests-axiom](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-axiom/), [c-agent-release-tests-axiom-valgrind](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-axiom-valgrind/), [c-agent-release-tests-daemon-tests](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-daemon-tests/)) jobs run our test suite in parallel as part of the [Build and test agent](https://source.datanerd.us/c-agent/c-agent/blob/master/jenkins/jobs/jobs.groovy#L48) phase of the `c-agent-cut-a-release` multiJob.
    - The [c-agent-release-branch](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-branch/) job creates a branch from the [c-agent/c-agent repository](https://source.datanerd.us/c-agent/c-agent) with a branch name of R`VERSION`.  Example: R1.0.0.
    - The [c-agent-release-tarball](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tarball/) job packages the C-agent as a tarball
    - The [c-agent-release-tarball](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tarball/) upload a tarball [to the testing server](http://nr-downloads-private.s3-website-us-east-1.amazonaws.com/75ac22b116/c_agent/), (in the future the multiJob will probably have a parameter to select `testing` or `production`).

3. Did all the [c-agent-cut-a-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-cut-a-release/) multiJob succeed (is it all blue icon in jenkins)?

4. Run the [c-agent-check-archive](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-check-archive/) job to download the tarball and confirm its possible to link and compile the test_app program with the `libnewrelic.a` file, and that the resulting binary run successfully.

5. Look at the jenkins console output for the `c-agent-check-archive` job ([example](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-check-archive/4/console)) for the test application staging URL, and confirm traffic is flowing.

6. Did the c-agent-check-archive succeed (blue icon in jenkins)?

7. If the release process and testing regime uncover problems that require code changes, make those changes on the release branch and then merge into master.

8. Tag the Release.

At this point the build has succeeded, and the archive's on S3, tests are passing, and there's a general feeling of confidence in the release.  It's time to tag the release!  First, get the commit hash of the release branch. One way to do this is by running these commands from the `c-agent` directory of a checked out repository.

    //these commands will fetch commit hash for a pretend 0.1.3 release
    $ git remote add upstream git@source.datanerd.us:c-agent/c-agent.git
    $ git rev-parse upstream/R0.1.3
    ae6n84716bf8af592bdd7639e371cb032ee8ced5

Once you have a commit hash, run the following commands, (replacing the `0.1.3` and `ae6n84716bf8af592bdd7639e371cb032ee8ced5` with your own values).

    git tag -a v0.1.3 -m "tagging c-agent release" ae6n84716bf8af592bdd7639e371cb032ee8ced5
    git push --tags

Navigate [to the GitHub repository](https://source.datanerd.us/c-agent/c-agent), and check that the new tag is reflected in the UI.

9. [For production only] Send an email to `agent-releases@newrelic.com` with the release notes.

10. [For production only] Are there new docs staged?  If so tell the `#documentation` `@hero` to release the docs!
