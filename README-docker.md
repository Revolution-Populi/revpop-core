# Docker Container

This repository comes with built-in Dockerfile to support docker
containers. This README serves as documentation.

## Dockerfile Specifications

The `Dockerfile` performs the following steps:

1. Obtain base image (phusion/baseimage:18.04-1.0.0)
2. Install required dependencies using `apt`
3. Add revpop-core source code into container
4. Update git submodules
5. Perform `cmake` with build type `Release`
6. Run `make` and `make_install` (this will install binaries into `/usr/local/bin`
7. Purge source code off the container
8. Add a local revpop user and set `$HOME` to `/var/lib/revpop`
9. Make `/var/lib/revpop` and `/etc/revpop` a docker *volume*
10. Expose ports `8090` and `2771`
11. Add default config from `docker/default_config.ini` and entry point script
12. Run entry point script by default

The entry point simplifies the use of parameters for the `witness_node`
(which is run by default when spinning up the container).

You can launch a build process with a command
```sh
$ docker build $REVPOP_CORE_DIR -t local/revpop-core:latest
```

### Supported Environmental Variables

* `$REVPOPD_SEED_NODES`
* `$REVPOPD_RPC_ENDPOINT`
* `$REVPOPD_PLUGINS`
* `$REVPOPD_REPLAY`
* `$REVPOPD_RESYNC`
* `$REVPOPD_P2P_ENDPOINT`
* `$REVPOPD_WITNESS_ID`
* `$REVPOPD_PRIVATE_KEY`
* `$REVPOPD_TRACK_ACCOUNTS`
* `$REVPOPD_PARTIAL_OPERATIONS`
* `$REVPOPD_MAX_OPS_PER_ACCOUNT`
* `$REVPOPD_ES_NODE_URL`
* `$REVPOPD_TRUSTED_NODE`

### Default config

The default configuration is:

    p2p-endpoint = 0.0.0.0:2771
    rpc-endpoint = 0.0.0.0:8090
    enable-stale-production = false
    max-ops-per-account = 1000
    partial-operations = true

# Docker Compose

With docker compose, multiple nodes can be managed with a single
`docker-compose.yaml` file:

    version: '3'
    services:
     main:
      # Image to run
      image: local/revpop-core:latest
      # 
      volumes:
       - ./docker/conf/:/etc/revpop/
      # Optional parameters
      environment:
       - REVPOPD_ARGS=--help

or

    version: '3'
    services:
     fullnode:
      # Image to run
      image: local/revpop-core:latest
      environment:
      # Optional parameters
       - REVPOPD_ARGS=--help
      ports:
       - "0.0.0.0:8090:8090"
      volumes:
      - "revpop-fullnode:/var/lib/revpop"


# Amazon Elastic Container Registry (ECR)

This container is properly registered with the Amazon ECR service:

* [revpop/revpop-core](https://gallery.ecr.aws/revpop/revpop-core)

Going forward, every release tag will be built into ready-to-run containers, there.

# Docker Compose

One can use docker compose to setup a trusted full node together with a
delayed node like this:

```
version: '3'
services:

 fullnode:
  image: public.ecr.aws/revpop/revpop-core:latest
  ports:
   - "0.0.0.0:8090:8090"
  volumes:
  - "revpop-fullnode:/var/lib/revpop"

 delayed_node:
  image: public.ecr.aws/revpop/revpop-core:latest
  environment:
   - 'REVPOPD_PLUGINS=delayed_node witness'
   - 'REVPOPD_TRUSTED_NODE=ws://fullnode:8090'
  ports:
   - "0.0.0.0:8091:8090"
  volumes:
  - "revpop-delayed_node:/var/lib/revpop"
  links: 
  - fullnode

volumes:
 revpop-fullnode:
```
