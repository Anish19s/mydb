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

## Requirements

- A C++ compiler with C++11 support (for example, `g++`).
- GNU Make.
- GNU Readline development library (`libreadline` and its headers).

On Debian or Ubuntu, install the build dependencies with:

```sh
sudo apt install build-essential libreadline-dev
```

## Build and run

Run the commands from this directory (`NITCbase/mynitcbase`):

```sh
make
./nitcbase
```

The `#` prompt accepts commands. Enter `HELP` to see the command list, or `EXIT` to quit. To compile with debug symbols:

```sh
make mode=debug
./nitcbase-debug
```

The database and CSV/batch file paths are relative to the current working directory. Running from `mynitcbase` lets the program find the sibling `Disk` and `Files` directories under `NITCbase`.

## Quick example

```sql
CREATE TABLE students (id NUM, name STR, score NUM);
OPEN TABLE students;
INSERT INTO students VALUES (1, Alice, 92);
INSERT INTO students VALUES (2, Bob, 78);
CREATE INDEX ON students.id;
SELECT * FROM students INTO passing WHERE score >= 80;
SELECT name, score FROM students INTO roster;
CLOSE TABLE students;
EXIT;
```

Selection and projection commands write their results into a new target relation. Target relations must not already exist. String and numeric values are entered without quotes; the command parser accepts simple alphanumeric values and decimal numbers.

## Command reference

Commands are case-insensitive, and the trailing semicolon is optional.

| Operation | Syntax |
| --- | --- |
| Create relation | `CREATE TABLE name (column NUM, label STR);` |
| Remove relation | `DROP TABLE name;` |
| Open / close relation | `OPEN TABLE name;` / `CLOSE TABLE name;` |
| Rename relation | `ALTER TABLE RENAME old_name TO new_name;` |
| Rename attribute | `ALTER TABLE name COLUMN old_column TO new_column;` |
| Add / remove index | `CREATE INDEX ON name.column;` / `DROP INDEX ON name.column;` |
| Insert one row | `INSERT INTO name VALUES (1, Alice);` |
| Import CSV rows | `INSERT INTO name VALUES FROM data.csv;` |
| Copy all columns and rows | `SELECT * FROM source INTO target;` |
| Project columns | `SELECT column1, column2 FROM source INTO target;` |
| Filter rows | `SELECT * FROM source INTO target WHERE column >= value;` |
| Join relations | `SELECT * FROM left JOIN right INTO target WHERE left.id = right.id;` |
| Run a batch | `RUN commands.txt;` |

Comparison operators for filtering are `=`, `!=`, `<`, `<=`, `>`, and `>=`. The join condition is an equality between one attribute from each relation. CSV imports look for files in `../Files/Input_Files/`; batch files are read from `../Files/Batch_Execution_Files/`.

You can also run a batch file when starting the program:

```sh
./nitcbase run s11test.txt
```

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

## Clean build artifacts

```sh
make clean
```
