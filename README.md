# NITCbase

A small relational database system written in C++. NITCbase implements a disk-backed block store, a buffer manager, relation and attribute catalogs, record operations, relational algebra, and B+ tree indexes. It includes an interactive SQL-like command line for working with relations.

## Features

- Create, open, close, drop, and rename relations and attributes.
- Store numeric (`NUM`) and string (`STR`) attributes.
- Insert records individually or import rows from CSV files.
- Select records using comparison conditions and project chosen attributes.
- Join two relations with an equijoin.
- Create and drop attribute indexes backed by B+ trees.
- Run command batches from a file.
- Store database pages in a fixed-size disk image and manage pages through a buffer layer.

## Project structure

```text
mynitcbase/
├── Algebra/           # Insert, select, project, and join operations
├── BlockAccess/       # Record and block-level access methods
├── BPlusTree/         # B+ tree index operations
├── Buffer/            # Block buffers and static buffer manager
├── Cache/             # Open relation and attribute metadata caches
├── Disk_Class/        # Disk image read/write interface
├── Frontend/          # Command-facing database operations
├── FrontendInterface/ # Interactive shell, parser, and help
└── Schema/            # Catalog, relation, and index management
```

The sibling `../Disk/` directory contains the disk image files. `../Files/` contains CSV inputs, batch scripts, and output files.


