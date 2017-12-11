# C Agent Release Checklist

## Creating a Release Build and Promoting to Test

- Create a Release Branch
    - @TODO: How to create branch and/or tags
    - @TODO: Or do we just go with master?    
    
- Update Jenkins Jobs to Point to new release branch
    - @TODO: How will this work
    - @TODO: How do we create/configure a version number for public consumption (i.e. C-Agent: 1.0.5)
- Run Jenkins Jobs
    - @TODO: Which jobs do we need to run?
    - @TODO: Do we run from UI? Via a shell script?  Via detecting a release branch ala the PHP Agent?    

## Promoting a Build from Test to Production

- Examine artifacts from *Creating a Release Build and Promoting to Test* and confirm they're ready for Release
    - @TODO: What additional testing do we need to do here?

- @TODO: What's involved in taking the build from test and marking it live?
