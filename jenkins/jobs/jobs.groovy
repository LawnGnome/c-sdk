import newrelic.jenkins.extensions
// Note:  Requires the Multijob plugin.  For this groovy file we are using Version 1.23

use(extensions) {
  def org = 'c-agent'
  def project = 'c-agent'
  def _repo = 'c-agent/c-agent'
  def _branch = 'master'
  def host = 'source.datanerd.us'
  def executeOn = 'ec2-linux'
  def versionDescription = 'Version is denoted as [Major].[Minor].[Patch] For example: 1.1.0'
  
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
      steps {
        reseedFrom('jenkins/jobs/*.groovy')
      }
    }
  }

  // configuration for the actual build jobs and multi-jobs below this comment

  // "Cutting a release" is a multijob that calls all of the necessary base jobs
  // to take what is on the master branch, build and test, create a release branch, and upload
  // to the appropriate S3 bucket.
  multiJob("$project-cut-a-release") {
    parameters {
      stringParam('VERSION', '', versionDescription)
    }

    steps {
      phase("Build Agent", 'SUCCESSFUL') {
        job("$project-release-build")
      }

      phase("Create a release branch", 'SUCCESSFUL') {
        job("$project-release-branch")
      }

      phase("Create a release tarball", 'SUCCESSFUL') {
        job("$project-release-tarball")
      }      
    }
  }

  // Builds the agent on the master branch and stores the artifacts for downstream jobs to consume
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

  // Creates a release branch on the master repo by taking what is currently at HEAD
  // on the master branch and pushing the contents to a branch named R$VERSION, where
  // $VERSION is the input parameter to this job.
  baseJob("$project-release-branch") {
    label "master"

    configure {

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
  
  // creates a tarball from 
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
          pattern('libnewrelic*.tar.gz')      
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
}
