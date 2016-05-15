# GT-RVM
Implement a recoverable virtual memory system like the ones described in the LRVM and Rio Vista papers. Users of your library can create persistent segments of memory and then access them in a sequence of transactions. Making the memory persistent is simple: mirror each segment of memory to a backup file on disk.
The difficult part is implementing transactions. If the client crashes, or if the client explicitly requests an abort, the memory should be returned to the state it was in before the transaction started. 

To implement a recoverable virtual memory system you should use one or more log files. Before writing changes directly to the backup file, you must first write the intended changes to a log file. Then, if the program crashes, it is possible to read the log and see what changes were in progress.


Commit of a transaction results in a redo log getting created on disk. This is faster than making changes to individual segments on disk due to latency issues in non-sequential writes. All the changes corresponding to a trasaction are written to disk in one go, which ensures the ACID properties of the transaction.

Structure of log fle maintained: Comma separetd list of tuples, where each tuple is:
trans_id, seg_name, start_addr, size, content
The truncate function is used to ensure that changes described in the redo-log are applied to corresponding segments, so that this log does not expand indefinitely.

------
Copyright (c) 2016 The persons identified as document authors. All rights reserved.

Code Components extracted from this document must include MIT License text.
