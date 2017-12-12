import newrelic.jenkins.extensions

use(extensions) {
  def org = 'c-agent'
  def project = 'c-agent'  
  def _repo = 'c-agent/c-agent'
  def _branch = 'master'
  
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
  baseJob("wip-archive-cagent") {      
    repo "c-agent/c-agent"
    branch "master"

    configure {

        parameters {
          stringParam('VERSION', 'X.X')      
        }

        publishers {    
            archiveArtifacts {
                pattern('libnewrelic*.tar.gz')        
                onlyIfSuccessful()
            }      
        }

        steps {
            copyArtifacts('c-agent-master') {                
                buildSelector {
                    latestSuccessful(true)
                }
            }

            shell('./jenkins/build/archive-artifacts.sh')
        }    
    }
  }
}