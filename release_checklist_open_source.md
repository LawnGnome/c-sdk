# C SDK Open Source Release Checklist

0. Are we ready to release?

   - Are there any [new agent features](https://newrelic.jiveon.com/people/zkay@newrelic.com/blog/2018/06/22/your-agent-features-are-our-ui-features-too)?

     If so, have [`agent_features.rb`](https://source.datanerd.us/APM/rpm_site/blob/master/app/models/agent_feature.rb)
     and [`agent_features.json`](https://source.datanerd.us/APM/agent-feature-list/blob/master/public-html/agent_features.json)
     been updated? (For *all* the new features?)

   - Prep the release notes, any relevant documentation changes and have them 
     ready for when we release.

1. Make a release commit.

   - Ensure New Feature and Bug Fix section of [CHANGELOG.md](https://source.datanerd.us/c-agent/c-agent/blob/master/CHANGELOG.md)
     reflects the reality of the new release

   - Update [VERSION](https://source.datanerd.us/c-agent/c-agent/blob/master/VERSION)
     file to the current release.

   - Copy the *Master* section of [CHANGELOG.md](https://source.datanerd.us/c-agent/c-agent/blob/master/CHANGELOG.md)
     to the current release version, add a new empty *Master* section to the top
     of the document.

2. Update public release and documentation branches.

   | WARNING: If files are added to the whitelist in [tools/update-public-branches.bash](tools/update-public-branches.bash) that were present in the repository during previous synchronizations, follow steps described in [adding previously blacklisted files](#adding-previously-blacklisted-files). |
   | --- |

   Start the process by building [c-sdk-cut-a-public-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-sdk-cut-a-public-release)
   with the `VERSION` number found in the [VERSION file](https://source.datanerd.us/c-agent/c-agent/blob/master/VERSION).
   Leave the  `GIT_REPO_BRANCH` set to `master`. The downstream jobs will 
   perform the following actions:

   - [c-sdk-update-public-branches](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-cut-a-public-release/):
     update the branches `public-branches/master` and
     `public-branches/gh-pages`.
   
   - [c-agent-compare-version](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-compare-version/): 
     sanity checks the version given as the input parameter versus what is found 
     in the VERSION file of the repository.
   
   - [c-agent-release-build](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-build/): 
     builds the agent by compiling the source tree and creating build artifacts, 
     based on `public-branches/master`.
   
   - [c-agent-release-tests-cmocka](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-cmocka/), 
     [c-agent-release-tests-axiom](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-axiom/),
     [c-agent-release-tests-axiom-valgrind](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-axiom-valgrind/),
     [c-agent-release-tests-daemon-tests](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tests-daemon-tests/): 
     run our test suite in parallel, based on `public-branches/master`.
   
   - [c-agent-release-branch](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-branch/):
     creates a release branch based on `master` with a branch name of 
     R`VERSION`. Example: `R1.0.0`.
   
   - [c-agent-release-tag](https://c-agent-build.pdx.vm.datanerd.us/job/c-agent-release-tag/): 
     tags the release branch.
   
3. Do manual tests.

   Base these tests on the updated `public-branches/master` branch.

   - Make sure the correct files are filtered out during the public branch
     update. This can conviently be done by inspecting the output of this
     command:
     ```
     git diff --name-only -r origin/master -r origin/public-branches/master
     ```
     `origin` is supposed to be the alias that refers to 
     `git@source.datanerd.us/c-agent/c-agent`. If this alias name differs in
     your case, replace the `origin` in the command above.

     If files are filtered out that shouldn't be filtered out, adapt 
     [update-public-branch.bash](./tools/update-public-branch.bash).

   - Build and run examples, check if things show up correctly in the UI. Check
     the documentation in `public-branches/gh-pages` for accuracy.


4. Push release and documentation branches to the public repository.

   - Make sure the branches `master` and `gh-pages` exist in the public
     repository.
   - Run [c-sdk-push-public-branches](https://c-agent-build.pdx.vm.datanerd.us/job/c-sdk-push-public-branches),
     once with the release branch `master` and once with the documentation
     branch `gh-pages` as argument for the parameter `GIT_REPO_BRANCH`.

5. Push Documentation Changes

   Push any related documentation changes to production. Let the hero in the
   [#documentation](https://newrelic.slack.com/messages/C0DSGL3FZ) room know the
   release related docs are ready to be released.

   If the only document to publish is the release notes, Agent engineers should
   be able to do this without help from the docs team.  Once you've set a
   release note's status as "Ready for Publication" and saved it, you should
   have access to set its status to "Published".

6. Send an email to `agent-releases@newrelic.com` with the release notes.

7. [For production only] Ensure that any new supportability metrics for the latest C SDK release have been added to [Angler](https://source.datanerd.us/agents/angler/).

## Adding previously blacklisted files

1. Make the sure the files to be added are not yet whitelisted in [tools/update-public-branches.bash](tools/update-public-branches.bash).

2. Run through step 2 from the list above (this step consists in running [c-sdk-cut-a-public-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-sdk-cut-a-public-release)).

3. Backup the files. Copy them to a destination outside your repository.

4. Remove the files from the branch that you want publish. Then commit, push
   and open a pull request.
   ```sh
   git rm <files>
   git commit <files>
   git push 
   ```

5. Once the pull request is merged, run through step 2 from the list above for
   the second time (this step consists in running [c-sdk-cut-a-public-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-sdk-cut-a-public-release)).

6. Copy the files back into the proper location inside the repository, add them
   with `git add`, also add them to the whitelist in [tools/update-public-branches.bash](tools/update-public-branches.bash).
   Then again commit, push and open a pull request.
   ```sh
   git add <files>
   vi tools/update-public-branches.bash
   git commit <files> tools/update-public-branches.bash
   git push 
   ```

7. Once the pull request is merged, again, run through step 2 from the list
   above for the third time (this step consists in running [c-sdk-cut-a-public-release](https://c-agent-build.pdx.vm.datanerd.us/job/c-sdk-cut-a-public-release)).

8. Continue with step 3 from the list above. Ask for a raise.
