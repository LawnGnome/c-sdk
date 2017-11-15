#!/bin/bash

#matching arrays of folders/repositories to deal with 
#the lack of hash-tables/dictionaries in bash < 4.0
declare -a subtree=(
  "php_agent/"
  "php_agent/axiom/tests/cross_agent_tests/"
  "php_agent/tests/include/newrelic-integration/")

declare -a repo=(
  "git@source.datanerd.us:php-agent/php_agent.git"
  "git@source.datanerd.us:agents/cross_agent_tests.git"
  "git@source.datanerd.us:php-agent/newrelic-integration.git")
  
#does initial adding of the subtrees. Only need to call once
add_subtrees()
{
    if [ -d "php_agent" ]
    then
        echo "ERROR: There's already a php_agent folder -- maybe you added the subtrees already?"
        exit 1
    fi
    #add php_agent subtree
    git subtree add --prefix=php_agent/ git@source.datanerd.us:php-agent/php_agent.git ${at_commit[0]} --squash    
    
    #remove the submodule entries from the subtree
    #so we can subtree them instead.  This will 
    #remove them only from our repo. We expect to see
    #   warning: Could not find section in .gitmodules where path=...
    #because the subtree command pulls in the submodule folder 
    #references, but does not update and .submodule folders
    
    git rm php_agent/axiom/tests/cross_agent_tests
    git rm php_agent/tests/include/newrelic-integration
    git commit . -m "removing sub module prior to adding subtree"
    
    #subtree the former submodules
    git subtree add --prefix=php_agent/axiom/tests/cross_agent_tests/ git@source.datanerd.us:agents/cross_agent_tests.git ${at_commit[1]} --squash    
    git subtree add --prefix=php_agent/tests/include/newrelic-integration/ git@source.datanerd.us:php-agent/newrelic-integration.git ${at_commit[2]} --squash        
}

#populates initial hash of commits on every program run
#will default to master unless someone's called set_commit_hashes
load_commit_hashes()
{
    #php-agent defaults to master, sub-modules default to the commits
    #where the PHP agent repo sub-modules are at (reported via)
    #git ls-tree HEAD axiom/tests/cross_agent_tests
    #git ls-tree HEAD tests/include/newrelic-integration    
    at_commit=(\
      "master"                                      #php-agent/php_agent
      "42b3a174b2fc1651ef87ffedea4adcbbe4e99d74"    #agents/cross_agents_tests
      "cbb30348b24df7daa89beb7f795a6e770e2cf523"    #php-agent/newrelic-integration
    )
    
    if [ -f /tmp/newrelic_cagent_subtree_hashes ]
    then
        # <3 bash: https://stackoverflow.com/questions/11393817/bash-read-lines-in-file-into-an-array
        IFS=$'\r\n' GLOBIGNORE='*' command eval  'at_commit=($(cat /tmp/newrelic_cagent_subtree_hashes))'
    fi
    
    #validate as "master" or a hash reference
    advice="Uset set_commit_hashes to fix, or remove /tmp/newrelic_cagent_subtree_hashes"
    for i in {0..2}
    do
      hash=${at_commit[$i]}
      if [ "$hash" = "master" ]
      then          
          continue;
      fi
      
      if [ ${#hash} -eq 40 ]
      then
          continue;
      fi
      echo "ERROR: commit hashes must be 40 character git commit hash, or the word 'master' "
      echo $advice
      exit 1
    done        
}
 
#runs through a list of repos and hashes and updates their version.yml fields acordingly
update_version_yml()
{
    for i in {0..2}
    do
        commitID=${at_commit[$i]}
      
        if [ "$commitID" = "master" ]
        then
            commitID=`git ls-remote ${repo[$i]} HEAD | grep -o '^\S*'`
        fi                
        folder=${subtree[$i]}  
        base=`basename ${folder}`
        echo "Creating/Replacing: $folder../vendor.yml"
        #create a new yaml file
        echo "vendor:" > $folder/../vendor.yml    
        #append folder and commit keys
        echo "    folder: $base" >> $folder/../vendor.yml    
        echo "    commit: $commitID" >> $folder/../vendor.yml
    done
    echo ""
    echo "IMPORTANT: Don't forget to commit your vendor.yml files"
}

#updates the subtrees 
update_subtrees()
{
    if [ ! -d "php_agent" ]
    then
        echo "ERROR: There's no php_agent folder -- you need to run add_subtrees first."
        exit 1
    fi
    for i in {0..2}
    do
        folder=${subtree[$i]} 
        git subtree pull --prefix=$folder ${repo[$i]} ${at_commit[$i]} --squash
    done
}

#shows what the next program run will use as commit hashes
view_commit_hashes()
{
    for i in {0..2}
    do
        echo ${repo[$i]} " will be added/updated at " 
        echo "    " ${at_commit[$i]}
        echo ""
    done
    echo "Use set_commit_hashes to change these values"    
}

#sets commit hashes for next program run
set_commit_hashes()
{
    #reset the file
    rm /tmp/newrelic_cagent_subtree_hashes
    for i in {0..2}
    do
        echo "Enter commit hash or 'master' for " ${repo[$i]}
        read tmp

        echo $tmp >> /tmp/newrelic_cagent_subtree_hashes
    done
    exit 1
}

#test, remove
#main program execution starts

#need to allow early call to set
if [ "$1" = "set_commit_hashes" ]
then 
    set_commit_hashes
    exit 0
fi

if [ "$0" != "./tools/manage-subtree.bash" ]
then
    echo "Please invoke from the root of the c-agent repo [./tools/manage-subtree.sh]"
fi 
  
#loads the commit hashes
at_commit=()
load_commit_hashes

#command execution tree and default usage info
if [ "$1" = "add_subtrees" ]
then
    add_subtrees
    update_version_yml
elif [ "$1" = "update_version_yml" ]
then 
    update_version_yml  
elif [ "$1" = "view_commit_hashes" ]
then 
    view_commit_hashes
elif [ "$1" = "update_subtrees" ]
then
    update_subtrees
    update_version_yml
else
    echo "USAGE: You can use this script to manage the subtree dependencies"
    echo "       from the PHP Agent repository.  "
    echo ""
    echo "       We'll call the add_subtrees command once to import the "
    echo "       subtrees and do some submodule cleanup."
    echo ""
    echo "       We'll call the update_subtrees command whenever we need"
    echo "       to sync the already imported/added subtrees with the PHP"
    echo "       agent repo.  You'll be prompted to set a commit message"
    echo "       for any subtree that needs an update.  Just use the default."    
    echo ""
    echo "       Both add_subtrees and update_subtrees commands will update/create"
    echo "       a set of versions.yml files.  To update these files manually"
    echo "       use the update_version_yml command"
    echo ""    
    echo "       To view the commit hashes that add_subtrees and update_subtrees will use"
    echo "       use the view_commit_hashes command."    
    echo ""    
    echo "       To **change** the commit hashes that add_subtrees and update_subtrees"
    echo "       will use, run set_commit_hashes. This will use a temp file to store the"
    echo "       new commit hashes."
    echo ""
    echo "CALLING: "
    echo ""   
    echo "       $ ./tools/manage-subtree.bash view_commit_hashes"     
    echo "       $ ./tools/manage-subtree.bash set_commit_hashes"         
    echo ""           
    echo "       $ ./tools/manage-subtree.bash add_subtrees"
    echo "       $ ./tools/manage-subtree.bash update_subtrees"    
    echo ""       
    echo "       $ ./tools/manage-subtree.bash update_version_yml"                 
fi