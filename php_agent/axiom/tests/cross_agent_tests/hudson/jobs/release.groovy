import newrelic.jenkins.extensions

def ownerAndRepo = 'php-agent/php_agent'
def branchSpec = '*/R5.0'

// How many builds to keep and for how long.
def daysToKeep = -1 // no age limit
def numToKeep = 5

use(extensions) {
  matrixJob('php-release-agent') {
    description 'Build the agent on the release branches.'

    logRotator(daysToKeep, numToKeep, -1, -1)

    repository(ownerAndRepo, branchSpec) {
      clean(true)
      pruneBranches(true)
      recursiveSubmodules(true)
    }

    // Parent build runs on master
    label('master')
    axes {
      label('label',
            'alpine-3-3-1',
            'centos5-32-nrcamp',
            'centos5-64-nrcamp',
            'macos106-64-nrcamp',
            'freebsd100-64-nrcamp')
    }

    triggers {
      githubPush()
    }

    steps {
      shell("./hudson/build.sh --optimize && ./hudson/test.sh --optimize")
    }

    publishers {
      warnings(['GNU C Compiler 4 (gcc)'])
      archiveArtifacts('releases/**/*')

      downstream('php-release-packages')
      downstream('php-release-tarballs')

      groovyPostBuild(readFileFromWorkspace('hudson/report-to-insights.groovy'))

      extendedEmail('php-agent@newrelic.com', '$DEFAULT_SUBJECT', '${SCRIPT, template="php-agent-html.template"}') {
        trigger('Failure')
        configure { node ->
          node / contentType << 'text/html'
          node / matrixTriggerMode << 'ONLY_PARENT'
        }
      }
    }

    configure { project ->
      project / buildWrappers / 'matrixtieparent.BuildWrapperMtp'(plugin: 'matrixtieparent@1.1') {
        labelName('master')
      }
    }
  }

  matrixJob('php-release-packages') {
    description 'Create the packages for the current release branches.'

    logRotator(daysToKeep, numToKeep, -1, -1)
    repository(ownerAndRepo, branchSpec) {
      clean(true)
      pruneBranches(true)
    }

    // Parent build runs on master
    label('master')
    // tieParentBuildToNode('master')

    axes {
      label('label',
            'centos5-32-nrcamp',
            'centos5-64-nrcamp',
            'ubuntu10-32-nrcamp',
            'ubuntu10-64-nrcamp')
    }

    // TODO(msl): It's unclear why either of these are necessary.
    blockOnUpstreamProjects()
    touchStoneFilter('label=="centos5-64-nrcamp"')

    steps {
      // Gather linux build artifacts. Copy artifacts from each label separately
      // to prevent Jenkins from creating a subdirectory for each label.
      for (lbl in ['centos5-32-nrcamp', 'centos5-64-nrcamp']) {
        copyArtifacts("php-release-agent/label=${lbl}") {
	  includePatterns('releases/**')
	  buildSelector {
	    upstreamBuild(true)
	  }
        }
      }

      shell('./hudson/package.sh')
    }

    publishers {
      archiveArtifacts('releases/*.rpm,releases/*.deb')

      extendedEmail('php-agent@newrelic.com', '$DEFAULT_SUBJECT', '${SCRIPT, template="php-agent-html.template"}') {
        trigger('Failure')
        configure { node ->
          node / contentType << 'text/html'
          node / matrixTriggerMode << 'ONLY_PARENT'
        }
      }
    }

    configure { project ->
      project / buildWrappers / 'matrixtieparent.BuildWrapperMtp'(plugin: 'matrixtieparent@1.1') {
        labelName('master')
      }
    }
  }

  freeStyleJob('php-release-tarballs') {
    description 'Create the tarballs for release branches.'

    logRotator(daysToKeep, numToKeep, -1, -1)
    repository(ownerAndRepo, branchSpec) {
      clean(true)
      pruneBranches(true)
    }

    label('ec2-linux')

    steps {
      def agentBuildLabels = [
        'alpine-3-3-1',
        'centos5-32-nrcamp',
        'centos5-64-nrcamp',
        'macos106-64-nrcamp',
        'freebsd100-64-nrcamp'
      ]

      // Copy artifacts from each label separately to prevent Jenkins from
      // creating a subdirectory for each label.
      for (lbl in agentBuildLabels) {
        copyArtifacts("php-release-agent/label=${lbl}") {
	  includePatterns('releases/**')
	  buildSelector {
	    upstreamBuild(true)
	  }
        }
      }

      shell('./hudson/package.sh')
    }

    publishers {
      archiveArtifacts('releases/*.tar.gz')
      emailOnFailure('php-agent@newrelic.com')
    }
  }
}
