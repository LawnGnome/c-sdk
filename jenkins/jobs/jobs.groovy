import newrelic.jenkins.extensions
// Note:  Requires the Multijob plugin.  For this groovy file we are using Version 1.23

use(extensions) {
  def org = 'c-agent'
  def project = 'c-agent'
  def _repo = 'c-agent/c-agent'
  def _branch = 'master'
  def executeOn = 'ec2-linux'

  //A jenkins user will create the initial version of this reseed 
  //job manually via the Jenkins UI.  Running that jobs the first
  //time will create the "$project-reseed-build" job.  Running
  //is subsequent times will update the "$project-reseed-build" 
  //jobs with any changes made to the baseJob configuration below
  baseJob("$project-reseed-build") {
    repo _repo
    branch _branch
    label "master"

    configure {
      steps {
        reseedFrom('jenkins/jobs/*.groovy')
      }
    }
  }

  //configuration for the actual build jobs and multi-jobs below this comment

  multiJob("$project-cut-a-release") {
    parameters {
      stringParam('VERSION', '', 'Version is denoted as [Major].[Minor].[Patch] For example: 1.1.0')
    }

    steps {
      phase("Build Agent", 'SUCCESSFUL') {
        job("$project-release-build")
      }
    }
  }

  baseJob("$project-release-build") {
    repo _repo
    branch _branch
    label executeOn

    configure {

      steps {
        shell('./jenkins/build/make.sh')
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
}
