import newrelic.jenkins.extensions

use(extensions) {
  baseJob("wip-archive-cagent") {      
    repo "c-agent/c-agent"
    branch "master"

    configure {
        publishers {    
            archiveArtifacts {
                pattern('libnewrelic.tar.gz')        
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
