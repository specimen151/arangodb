How to run ArangoDB in a Docker container
=========================================

Problem
-------

How do you make ArangoDB run in a Docker container?

Solution
--------

ArangoDB is now available as an [official repository in the Docker Hub](https://hub.docker.com/_/arangodb/) (@see documentation there).

Persistence on OSX
---
When mapping folders on OSX, remember that only folders which are set up as shared under File Sharing in the Docker Mac client can be mapped. Other folders will be mapped inside the enclosing VM, in which Docker is running.
Also keep in mind that the folder is /var/lib/arangodb3 (there is some older docs out there that can be confusing).

**Author:** [Frank Celler](https://github.com/fceller)

**Tags:** #docker #howto
