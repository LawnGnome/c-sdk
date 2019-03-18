# Guidelines for Preparing the PHP Agent Repository for Source Availability

## Remove TODO(xyz) comments

There are many comments throughout the repository that have the form,

```
  /* 
   * TODO(xyz): Here is an idea for making this particular
   * chunk of code better.  Or, this is why I think
   * this code is terrible.
   */
```

For example, in `php_api.c`,

```
  /*
   * TODO(rrh): This is really weak argument parser.
   * It should accept exactly 0 or 1 arguments,
   * and give up if the argument isn't "b" or "l".
   */
```

These `TODO` comments must be removed from the repository.  Use the 
following steps to clean up these kinds of comments, erring on the
side of aggressive deletion.

1. Are the initials of someone who still works on the team? _Let them
clean up the comment_.
2. Does the comment seem out of context or irrelevant now? _Delete the
entire comment.  Congratulations on decluttering!_
3. Does the comment offer a Really Good Idea? Does the Really Good Idea
seem possible to implement given everything else on the product
roadmap?  _Make a Jira and delete the comment._

## Remove credentials and New Relic endpoint URLs

Remove any hard coded user names, passwords, endpoint URLs, SSH keys and IP
addresses from code, documentation and examples.

When in doubt, check our [Data Classification and handling policy](https://newrelic.atlassian.net/wiki/spaces/INFOSEC/pages/13271361/New+Relic+Data+Classification+and+Handling) 
and/or with our app security team.

## Remove offensive language

Including, but not limited to:
- complaints about other teams,
- complaints about other products,
- complaints about competitors,
- most swearing (except when absolutely necessary, such as anything to do with
Jenkins).

## Remove references to NRCAMP or NRLAMP

To avoid customer requests like: "NRLAMP seems to be a great tool, how can I
set it up"?

## Remove person names/initials and product codenames

## Adhere to a consistent coding standard

[This](https://newrelic.jiveon.com/docs/DOC-65673) can serve as a start, and
can be extended and adapted as we go.
