# C Agent Release Checklist

1. Make a release commit

- Update [VERSION](https://source.datanerd.us/c-agent/c-agent/blob/master/VERSION) file to the current release
- Update [CHANGELOG.md](https://source.datanerd.us/c-agent/c-agent/blob/master/CHANGELOG.md)

2. Kick off Jenkins

- Start the process by building [c-agent-cut-a-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-cut-a-release/build?delay=0sec) with the `VERSION` number found in the VERSION file.  The downstream jobs will perform the following actions

    - [Build the C-agent](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-build/)
    - @TODO: Test the C-agent (Break into mutliple lines?  Example: `C unit tests`, `Axiom unit tests`, etc)
    - [Create a release branch](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-branch/) on the master repository with a name of R`VERSION`.  Example:  R1.0.0
    - [Package the C-agent](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tarball/) as a tarball
    - @TODO Upload tarball to testing server (In the future the multiJob will probably have a parameter to select `testing` or `production`)

3. Are all the [Jenkins CI jobs](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-cut-a-release/) blue?

4. [For production only] Send an email to `agent-releases@newrelic.com` with the release notes

5. [For production only] Are there new docs staged?  If so tell the `#documentation` `@hero` to release the docs!
