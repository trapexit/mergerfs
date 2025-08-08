# Resource Usage

* Configurable number of threads
    * reading from kernel
    * processing messages from kernel
    * readdir concurrency
* 1MB+ pre reader thread + inflight processing for messages depending on fuse_msg_size
* Buffers allocated temporarily for reading directories
* gidcache
* FUSE nodes
* noforget forgotten nodes
* Sets oom_score_adj to -990 to limit OOM Killer risk
* Increases available file descriptor and file size limit for itself
* Lowers scheduling priority to -10 (by default)
* readahead values of mergerfs itself and managed filesystems can be modified
