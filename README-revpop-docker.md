1. Build the revpop-core image (REVPOP_CORE_DIR is a path where you cloned the repository):
```
$ docker build $REVPOP_CORE_DIR -t revpop-core:latest --iidfile ./iid.txt
```
2. Run a new container using revpop-core image:
```
$ docker run --rm -i -t -p 8090:8090 $(cat ./iid.txt)
```

