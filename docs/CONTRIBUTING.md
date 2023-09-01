## XRootD Development Model and How to Contribute

### Versioning

XRootD software releases are organized into major, minor and patch versions,
with the intent that installing minor version releases with the same major
version do not take long to perform, cause significant downtime, or break
dependent software. New major versions can be more disruptive, and may
substantially change or remove software components. Releases are assigned
version numbers, such as "5.6.0", where:

* The first number designates a major version. Major versions may introduce
  binary incompatibility with previous major versions and may require code
  dependent on libraries in the new major version to be recompiled. Generally,
  such requirements are limited to code that enhances XRootD functionality (e.g.
  plug-ins). User application code that only uses public APIs should continue
  to work unchanged. Consequently, major versions are infrequent and are
  introduced approximately every 5 years.

* The second number increments within the major version and designates a minor
  version. Minor versions introduce new features within a major version. They
  are binary compatible with all versions within the major version and occur as
  often as needed to address community needs. On average, there are a few minor
  versions per year.

* The last digit increments whenever changes are applied to a minor version to
  fix problems. These occur at random frequency, as often as necessary to fix
  problems. Since patch versions represent the minimum change necessary to fix
  a problem they provide the forward path for problem resolution.

* When the first number increments, the second and third numbers are reset to
  zero and when the second number increments the third number is reset to zero.

* A fourth number may be added by EOS as indication that the version of XRootD
  used by EOS has been patched after the official release. Patches introduced
  in an intermediate release for EOS will be likely included into the following
  patch release, unless it is a temporary fix affecting only EOS.

#### Library versions

When a library evolves compatibly: existing interfaces are preserved, but new
ones are added the library’s minor version number must be incremented. Since
nothing has been done that would break applications constructed earlier, it is
OK for older applications to be linked with the newer library at run-time.

If the interfaces in a library shared object change incompatibly, then the major
revision number associated with the library must be incremented. Doing so will
cause run-time linking errors for the applictions constructed with the older
versions of the library and thus will prevent them from running, as opposed to
crashing in an uncontrollable way.

More information on library versioning is available
[here](https://www.usenix.org/legacy/publications/library/proceedings/als00/2000papers/papers/full_papers/browndavid/browndavid_html/)
and
[here](https://www.akkadia.org/drepper/dsohowto.pdf).

The project policy is that a change to public interfaces (as defined in the
installed headers) requires a major release - bumping the major version number.

### Releases and Release Procedure

Feature releases with current developments will normally be built a few times 
per year. Each `master` release is preceded by one or more release candidates
that get tested for bugs and deployment issues in a possibly wide range of
environments. When the release candidates are deemed sufficiently stable, then 
the final release is built.

In addition to the `master` or "feature" releases, "bug fix" releases may be built
whenever needed. These are for bug fixes only, so they normally should not need
release candidates (due to the reduced need for additional testing).

RPM packages are built for each release, including release candidates. All the
packages are pushed to the testing yum repository. Additionally, all the bug fix
releases and all the final `master` releases are pushed to the stable repository.
See the [download](https://xrootd.slac.stanford.edu/dload.html) page for details.

### Stable and Develoment Branches

Beginning with XRootD 5.6.0, the development model is based on two long-term
branches: `master`, and `devel`.

The `master` branch is the stable branch. It contains released versions of
XRootD and may also contain unreleased bug fixes which do not require a new
minor release. Each patch release for a given major+minor series is created from
the `master` branch by adding any required bug fixes from the `devel` branch to
the `master` branch and tagging a new release, such that all XRootD releases may
be found linearly in git history.

The `devel` branch is the development branch where all new features and other
developments happen. Each new feature release is created by rebasing, then
(perharps partially) merging the `devel` branch into the `master` branch, then
tagging the relase on `master`. The `devel` branch will be kept current with the
`master` branch by rebasing it after each patch release, to ensure that all bug
fixes are always included in both `master` and `devel`.

### Guidelines for Contributors

This section provides guidelines for people who want to contribute
code to the XRootD project. It is adapted from git's own guidelines
for contributors, which can be found in their repository on GitHub at
https://github.com/git/git/blob/master/Documentation/SubmittingPatches.

#### Deciding what to base your work on

In general, always base your work on the oldest branch that your
change is relevant to.

* A bug fix should be based on the latest release tag in general. If
  the bug is not present there, then base it on `master`. Otherwise,
  if it is only present on `devel`, or a feature branch, then base it
  on the tip of `devel` or the relevant feature branch.

* A new feature should be based on `devel` in general. If the new
  feature depends on topics which are not yet merged, fork a branch
  from the tip of `devel`, merge these topics to the branch, and work
  on that branch.  You can get an idea of how the branches relate to
  each other with `git log --first-parent master..` or with
  `git log --all --decorate --graph --oneline`.

* Corrections and enhancements to a topic not yet merged into `devel`
  should be based on the tip of that topic. Before merging, we recommend
  cleaning up the history by squashing commits that are fixups for
  earlier commits in the same branch rather than committing a bad change
  and the fix for it in separate commits. This is important to preserve
  the ability to use git bisect to find which commit introduced a bug.

#### Make separate commits for logically separate changes

In your commits, you should give an explanation for the change(s) that
is detailed enough so that a code reviewer can judge if it is a good
thing to do or not without reading the actual patch text to determine
how well the code actually does it.

If your description is too long, that's probably a sign that the commit
should be split into finer grained pieces. That being said, patches which
plainly describe the things that help reviewers checking the patch and
future maintainers understand the code are very welcome.

If you are fixing a bug, it would be immensely useful to include a test
demonstrating the problem being fixed, so that not only the problem is
avoided in the future, but also reviewers can more easily verify that the
proposed fix works as expected. Similarly, new features which come with
accompanying tests are much more likely to be reviewed and merged in a
timely fashion.

When developing XRootD on your own fork, please make sure that the
existing test suite is not broken by any of your changes by pushing to
a branch in your own fork and checking the result of the GitHub Actions
runs.

#### Describe your changes well

The log message that explains your changes is just as important as the
changes themselves. The commit messages are the base for creating the
release notes for each release. Hence, each commit message should clearly
state whether it is a bug fix or a new feature whenever that is not
immediately obvious from the nature of the change itself. Moreover, it is
very important to explain not only _what_ your code does, but also _why_
it does it.

The first line of the commit message should be a short description of up
to about 50 characters (soft limit, hard limit at 80 characters), and
should skip the full stop. It is encouraged, although not necessary, to
include a tag which identifies the general area of code being modified,
for example "[Server]", "[XrdHttp]", etc. If in doubt, please check the
git log for similar files to see what are the current conventions.

After the title sentence, you should include a blank line and then the
body of the commit message, which should be a meaningful description that

* explains the problem the change tries to solve, i.e. what is wrong
  with the current code without the change.

* justifies the way the change solves the problem, i.e. why the
  result with the change is better.

* alternate solutions considered but discarded, if any.

You should use the imperative to describe your changes, for example:
```
Change default value of foo to 1024
```
instead of
```
This commit changes the default value of foo to 1024
```
or
```
Changed default default value of foo to 1024
```

Examples of good commit messages:

```
Author: Andrew Hanushevsky <abh@slac.stanford.edu>
Date:   Thu Jun 8 18:06:01 2023 -0700

    [Server] Allow generic prepare plug-in to handle large responses, fixes #2023
```

```
Author: Brian Bockelman <bbockelman@morgridge.org>
Date:   Sat Feb 18 13:15:49 2023 -0600

    Map authentication failure to HTTP 401

    The authentication failure error message was previously mapped to
    HTTP 500 (internal server error).  401 Unauthorized (despite its name)
    is what HTTP servers typically utilize for authentication problems.
```

#### References to other commits, issues, pull requests, etc

Sometimes, it may be useful to refer to the pull request on GitHub, an
open issue which a commit fixes/closes, or simply an older commit which
may have introduced a regression fixed by the current change. When referring
to older commits, try to use the same format as produced by
```
git show -s --pretty=reference <commit>
```

For issues, add a "Closes: #nnnn" or "Fixes: #nnnn" tag to the body of the
commit message (or, even better, to the pull request description). When
linking a change to a specific issue or pull request, please verify in the
GitHub website that the association actually worked. Depending on how you
phrase your message, this may not happen automatically. In that case, it is
also possible to use the "Development" side panel on the right to manually
create the connection between pull requests and issues. If you intend to
have your changes be part of a particular release which is not the next
release being planned, you may also mark your pull request for inclusion
in the desired release by using the "Milestone" side panel on the right.
This can be used as an alternative method of marking a change as "bug fix"
or "feature", depending on if it will only be included in the next patch
release or feature release. Any changes which require a major release must
be marked with the appropriate milestone.
