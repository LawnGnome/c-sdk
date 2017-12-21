import newrelic.jenkins.extensions
// Note:  Requires the Multijob plugin.  For this groovy file we are using Version 1.23
// https://plugins.jenkins.io/jenkins-multijob-plugin

use(extensions) {
  def org = 'c-agent'
  def project = 'c-agent'
  def _repo = 'c-agent/c-agent'
  def _branch = 'master'
  def host = 'source.datanerd.us'
  def executeOn = 'ec2-linux'
  def versionDescription = 'Version is denoted as [Major].[Minor].[Patch] For example: 1.1.0'
  def actions = [
    'release-2-testing',
  ]

  // A jenkins user will create the initial version of this reseed
  // job manually via the Jenkins UI.  Running that jobs the first
  // time will create the "$project-reseed-build" job.  Running
  // is subsequent times will update the "$project-reseed-build"
  // jobs with any changes made to the baseJob configuration below
  baseJob("$project-reseed-build") {
    repo _repo
    branch _branch
    label "master"

    configure {
      description('Take all of the groovy files found in jenkins/jobs/*.groovy and reseed them.')

      steps {
        reseedFrom('jenkins/jobs/*.groovy')
      }
    }
  }

  // Configuration for the actual build jobs and multi-jobs below this comment

  // "Cutting a release" is a multijob that calls all of the necessary base jobs
  // to take what is on the master branch, build and test, create a release branch, and upload
  // to the appropriate S3 bucket.
  multiJob("$project-cut-a-release") {
    parameters {
      stringParam('VERSION', '', versionDescription)
      choiceParam('ACTION', actions, 'Required -- where to publish the tarball.')
    }

    steps {
      phase("Build and test agent", 'SUCCESSFUL') {
        job("$project-compare-version")
        job("$project-release-build")
        job("$project-release-tests-cmocka")
        job("$project-release-tests-axiom")
        job("$project-release-tests-axiom-valgrind")
        job("$project-release-tests-daemon-tests")
      }

      phase("Create a release branch", 'SUCCESSFUL') {
        job("$project-release-branch")
      }

      phase("Create a release tarball", 'SUCCESSFUL') {
        job("$project-release-tarball")
      }

      phase("Publish archive to S3", 'SUCCESSFUL') {
        job("$project-publish")
      }

      phase("Reindex S3 Bucket", 'SUCCESSFUL') {
        job("$project-reindex")
      }
    }
  }

  // The PR multijob calls all of the testing base jobs required to identify
  // this commit as clean and valid.
  multiJob("$project-pullrequest") {
    description('When the PR trigger is exercised test the agent by running the below jobs.')

    repositoryPR(_repo)

    triggers {
      pullRequest {
        admins(['aharvey', 'rvanderwal', 'rlewis', 'tcrenshaw', 'astorm'])
        orgWhitelist(org)
        triggerPhrase("\\Qok jenkins\\E")
        permitAll()
        useGitHubHooks()
      }
    }

    steps {
      phase("Test Agent", 'SUCCESSFUL') {
        job("$project-release-tests-cmocka")
        job("$project-release-tests-axiom")
        job("$project-release-tests-axiom-valgrind")
        job("$project-release-tests-daemon-tests")
      }
    }
  }

  //gives the s3 buckets index.html files
  baseJob("$project-reindex") {
    repo "astorm/s3-index"
    branch "master"
    label executeOn

    configure {
        description('Reindex the S3 bucket so new build files show up on index.html files')

        environmentVariables {
          env('AWS_DEFAULT_REGION', 'us-east-1')
          env('AWS_ACCESS_KEY_ID', '$PHP_DEPLOY_ACCESS_KEY_ID')
          env('AWS_SECRET_ACCESS_KEY', '$PHP_DEPLOY_SECRET_ACCESS_KEY')
        }

        steps {
          shell("GOPATH=\$( pwd ) /usr/local/go/bin/go get github.com/aws/aws-sdk-go/..." + "\n" +
                "GOPATH=\$( pwd ) /usr/local/go/bin/go run src/indexer/main.go -bucket nr-downloads-private -prefix 75ac22b116 -upload")
        }

        buildInDockerImage('./docker')
    }
  }

  // Compare the input parameter `VERSION` with the value inside the VERSION folder.  A sanity check job.
  baseJob("$project-compare-version") {
    repo _repo
    branch _branch
    label executeOn

    configure {
      description('Compare the input parameter $VERSION with the value inside the VERSION folder.  A sanity check job.')

      parameters {
        stringParam('VERSION', '', versionDescription)
      }

      steps {
        shell('[[ "x${VERSION}" = "x$(cat VERSION)" ]]')
      }
    }
  }

  // Builds the agent on the master branch and stores the artifacts for downstream jobs to consume
  baseJob("$project-release-build") {
    repo _repo
    branch _branch
    label executeOn

    configure {
      description('Build the agent on the master branch and store the artifacts for downstream jobs to consume.')

      steps {
        shell("source ./jenkins/build/shared.sh"  + "\n" +
              "make clean" + "\n"                 +
              "make -j\$(nproc) all daemon")
      }

      publishers {
        archiveArtifacts {
          pattern('libnewrelic.h')
          pattern('libnewrelic.a')
          pattern('php_agent/bin/daemon')

          onlyIfSuccessful()
        }
      }

      buildInDockerImage('./jenkins/docker')
    }
  }

  // Creates a release branch on the master repo by taking what is currently at HEAD
  // on the master branch and pushing the contents to a branch named R$VERSION, where
  // $VERSION is the input parameter to this job.
  baseJob("$project-release-branch") {
    label "master"

    configure {
      description('Create a release branch on the master repository that represents what is currently at HEAD.  Branch will be named R$VERSION.')

      parameters {
        stringParam('VERSION', '', versionDescription)
      }

      scm {
        git {
          remote {
            url("git@" + host + ":" + _repo + ".git")
            credentials("artifactory-jenkins-build-bot")
          }
          branch(_branch)
          localBranch('R$VERSION')
        }
      }

      publishers {
        git {
          branch('origin', 'R$VERSION')
        }
      }
    }
  }

  // Creates a tarball from
  // 1. Files from the previously successful build steps
  // 2. Checking out the previously created release branch
  baseJob("$project-release-tarball") {
    repo _repo
    branch 'R$VERSION'
    label executeOn

    configure {
      description('Creates a release tar.gz archive from previous build and release branch files.')

      parameters {
        stringParam('VERSION', '', versionDescription)
      }

      publishers {
        archiveArtifacts {
          pattern('libnewrelic*.tgz')
          onlyIfSuccessful()
        }
      }

      steps {
        copyArtifacts("$project-release-build") {
          buildSelector {
            latestSuccessful(true)
          }
        }

        shell('./jenkins/build/archive-artifacts.sh')
      }
    }
  }

  baseJob("$project-release-tests-cmocka") {
    repo _repo
    branch _branch
    label executeOn

    configure {
      description('Run the cmocka test suite in the HBB container.')

      steps {
        shell("source ./jenkins/build/shared.sh"  + "\n" +
              "make clean" + "\n"                 +
              "make run_tests")
      }

      buildInDockerImage('./jenkins/docker')
    }
  }

  baseJob("$project-release-tests-axiom") {
    repo _repo
    branch _branch
    label executeOn

    configure {
      description('Run the axiom test suite in the HBB container.')

      steps {
        shell("source ./jenkins/build/shared.sh"  + "\n" +
              "make clean" + "\n"                 +
              "make -C php_agent axiom-run-tests")
      }

      buildInDockerImage('./jenkins/docker')
    }
  }

  baseJob("$project-release-tests-axiom-valgrind") {
    repo _repo
    branch _branch
    label executeOn

    configure {
      description('Run the axiom test suite under valgrind in the HBB container.')

      steps {
        shell("source ./jenkins/build/shared.sh"  + "\n" +
              "make clean" + "\n"                 +
              "make -C php_agent axiom-valgrind")
      }

      buildInDockerImage('./jenkins/docker')
    }
  }

  baseJob("$project-release-tests-daemon-tests") {
    repo _repo
    branch _branch
    label executeOn

    configure {
      description('Run the daemon go tests in the HBB container.')

      steps {
        shell("source ./jenkins/build/shared.sh"  + "\n" +
              "make clean" + "\n"                 +
              "make -C php_agent/ daemon_integration")
      }

      buildInDockerImage('./jenkins/docker')
    }
  }

  // Publish the archived build artifacts to the repository indicated by ACTION.
  baseJob("$project-publish") {
    repo _repo
    branch 'R$VERSION'

    label executeOn

    configure {
      description('Publish a C-Agent release to the testing repository.')

      parameters {
        stringParam('VERSION', '', versionDescription)
        choiceParam('ACTION', actions, 'Required -- the action to perform.')
      }

      environmentVariables {
        env('AWS_DEFAULT_REGION', 'us-east-1')
        env('AWS_ACCESS_KEY_ID', '$PHP_DEPLOY_ACCESS_KEY_ID')
        env('AWS_SECRET_ACCESS_KEY', '$PHP_DEPLOY_SECRET_ACCESS_KEY')
      }

      steps {

        // Only run this job if the user selected release-2-testing.  Future versions of this
        // job will see a choice for testing-2-production.
        conditionalSteps {
          condition {
            stringsMatch('${ACTION}', "release-2-testing", true)
          }
          runner('DontRun')

          steps {

            // Copy the .tar.gz file from an upstream project.
            copyArtifacts("$project-release-tarball") {
              includePatterns('*.tgz')
              buildSelector {
                latestSuccessful(true)
              }
            }

            shell(  "PATH=/bin:\$PATH" + "\n" +
                    './jenkins/build/publish.sh --action="$ACTION" --version="$VERSION"')

           }

           buildInDockerImage('./jenkins/docker-cli')
        }
      }
    }
  }
}
