# FUSE lib ram file system

* Dependencies: libfuse3-dev (there is cmake/FindFUSE3.cmake)
    * cmake/FindFUSE3.cmake finds it in local file system

* Build cmake/make

```
mkdir build && cd build
cmake ..
make
```

* Output:
    * ramfs - high level API implementation
    * ramfs_ll - low level level API implementation

* TODO:
    * chmod/chown
    * obey permissions
    * fix log-level mkdir FUSE error (no clue why)
