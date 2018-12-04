# How to Contribute

We'd love to accept your patches and contributions to this project. There are
just a few small guidelines you need to follow.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License
Agreement. You (or your employer) retain the copyright to your contribution;
this simply gives us permission to use and redistribute your contributions as
part of the project. Head over to <https://cla.developers.google.com/> to see
your current agreements on file or to sign a new one.

You generally only need to submit a CLA once, so if you've already submitted one
(even if it was for a different project), you probably don't need to do it
again.

## Code reviews

All submissions, including submissions by project members, require review. We
use GitHub pull requests for this purpose. Consult
[GitHub Help](https://help.github.com/articles/about-pull-requests/) for more
information on using pull requests.

## Code style, structure & principles

Please keep consistent with the existing code style, which is inspired
by (the non-C++-specific parts of) the [Google C++ Style
Guide](https://google.github.io/styleguide/cppguide.html), with a few
exceptions: no mixed-case names, and a 100-character line length
limit.

The goal of the project is to be lightweight and have no large
external dependencies. If you wish to add a collector that inherently
depends on a library, it should be optional to the build, and probably
not included by default.

All exported metrics should be related to the node (host, machine...)
the exporter is running on, not to any specific service running on
it. Services should optimally export their own metrics natively, or
via an exporter specific to the service. Alternatively, you might be
interested in [mtail](https://github.com/google/mtail), which extracts
monitoring data from log files.

## Community Guidelines

This project follows [Google's Open Source Community
Guidelines](https://opensource.google.com/conduct/).
