JBOD Storage System

A networked storage system that combines 16 military-grade disks into a single 1MB storage device. Features block-level operations, write permissions, caching, and remote access through a custom network protocol.
Overview

This project turns multiple physical disks into one unified storage device. It works with 16 military-grade disks, each containing 256-byte blocks, adding up to 1MB total storage. The system uses a JBOD (Just a Bunch of Disks) setup with some cool features to make it fast and reliable.
Key Features

The storage system lets you read and write data at the block level with permission controls. It uses a caching system with a Most Recently Used (MRU) policy to speed things up by reducing how often it needs to access the disks. The system keeps data consistent by using write-through caching.
The networking part lets you access storage remotely through a custom client-server setup, handling all the communication between clients and the storage system securely and efficiently.

Technicals:
The system works with 256-byte blocks across 16 disks, handling read/write operations carefully to avoid errors. The cache can be sized from 2 to 4096 entries and keeps track of how well it's performing. Network operations use a special packet format to handle remote storage access efficiently.

Requirements:
C compiler with C11 support
POSIX-compliant operating system
Network connectivity for remote operations
Enough memory for cache operations

Building and Testing

bash

make clean

make

./tester

For network testing:

bash

./jbod_server

./tester -w traces/random-input -s 1024
