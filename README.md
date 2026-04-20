PES-VCS: Version Control System Implementation
Student Name: Anushka

SRN: PES1UG24CS074

Course: Operating Systems

Platform: WSL (Ubuntu 22.04)

1. Project Overview
This project involves building a simplified version of Git (PES-VCS) from scratch. The system implements a content-addressable filesystem, directory snapshots (trees), a staging area (index), and commit history. It demonstrates core OS concepts such as file descriptors, atomic operations, and filesystem organization.

2. Phase 1: Object Storage (Foundation)
In this phase, I implemented the object_write and object_read functions. This handles the conversion of data into unique objects identified by SHA-256 hashes.

Screenshots
Screenshot 1A: Phase 1 Test Results

Description: Output of ./test_objects confirming successful writing, deduplication, and integrity verification.
![1A - Test Objects Output
<img width="1291" height="268" alt="image" src="https://github.com/user-attachments/assets/923ae432-1769-4dee-9884-6726261cb216" />

Screenshot 1B: Sharded Directory Structure

Description: Result of find .pes/objects -type f showing how objects are stored in subdirectories (e.g., 0a/, f2/) to optimize filesystem performance.
![1B - Directory Sharding]
<img width="1291" height="268" alt="image" src="https://github.com/user-attachments/assets/be5944d8-7207-4477-992f-53d53fb566db" />


3. Phase 2: Tree Objects (Snapshots)
This phase involved mapping the staged files into a Tree object. The tree acts as a snapshot of a directory at a specific point in time.

Screenshots
Screenshot 2A: Phase 2 Test Results

Description: Output of ./test_tree confirming that the tree serialization and parsing logic is correct.
![2A - Test Tree Output]

<img width="948" height="172" alt="image" src="https://github.com/user-attachments/assets/2fee4489-5d11-487f-89d7-063812bd23f0" />

Screenshot 2B: Hex Dump of a Tree Object

Description: This screenshot shows the raw binary data of a generated tree object using the xxd tool. It confirms the internal format of the VCS snapshots.
![2B - Tree Hex Dump]
<img width="1706" height="402" alt="image" src="https://github.com/user-attachments/assets/75b0ec7e-fb76-44d6-a360-5b08dece8c62" />


4. Phase 3 & 4: Indexing and Commits
This phase implemented the staging area (index_add) and the creation of commit objects which link snapshots to authors and timestamps.

Screenshots
Screenshot 3A: Staging a File

Description: Output of ./pes status after adding a file, showing it as "staged".
![3A - Pes Status]
<img width="1207" height="358" alt="image" src="https://github.com/user-attachments/assets/bde1f642-2dd8-479a-a288-b41ceb06b5ac" />

Screenshot 3B: Staging Area Manifest (.pes/index)

Description: Analysis of the .pes/index file contents, showing the metadata tracked by the staging area.
![3B - Index Content Analysis]
<img width="1341" height="147" alt="image" src="https://github.com/user-attachments/assets/88379dbb-0a4b-4e46-b094-e0bdc31a426a" />

Screenshot 4A: Commit History

Description: Output of ./pes log showing a sequence of commits with hashes, author info, and messages.
![4A - Pes Log]
<img width="1457" height="406" alt="image" src="https://github.com/user-attachments/assets/c4b799a3-209b-410a-a089-85cdc6ee4618" />

Screenshot 4B: Final Integration

Description: Output of make test-integration showing the full system working end-to-end.
![4B - Integration Test Passed]

<img width="665" height="965" alt="image" src="https://github.com/user-attachments/assets/6df5f08e-39d5-432c-be04-71b65b0465ad" />


5. Analysis Questions & Answers
Q1: Why do we shard the object directory (using the first 2 characters of the hash)?
Answer: Sharding is a filesystem optimization technique. Many filesystems (like ext4) experience performance degradation when a single directory contains thousands of files. By using the first two characters of the hash as a subdirectory name, we distribute the files into 256 possible buckets (00 through ff), significantly reducing the number of files per directory and speeding up file lookups.

Q2: Explain the "Atomic Write" process used in object_write.
Answer: To prevent data corruption in case of a system crash, we use the "Write-to-Temp-and-Rename" pattern. We first write the data to a .tmp file. Only after the file is fully written and flushed to disk using fsync(), we call rename(). On Linux/POSIX systems, rename() is an atomic operation, meaning the object will either exist in its complete form or not at all; there is no "half-written" state.

Q3: What is the role of the index file in a VCS?
Answer: The index (or staging area) acts as a middle-man between the working directory and the repository history. It allows the user to carefully curate which changes should be part of the next commit. From an OS perspective, it is a flat-file database that stores file metadata (size, mtime, and hash) to detect modifications without re-reading the entire file content every time.

Q4: How does this VCS handle deduplication?
Answer: This is a content-addressable system. Because the filename of an object is the SHA-256 hash of its content, two different files with the exact same content will result in the same hash. When object_write is called, it checks object_exists(). If the hash is already present, it simply returns without writing anything new. This saves significant disk space.

6. Conclusion
Through this lab, I successfully implemented a functional Version Control System. The project provided deep insights into how high-level tools like Git manage data integrity and performance using low-level Operating System primitives.
