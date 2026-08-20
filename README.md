This is a plugin for deadbeef to track play count and last played time.
Made mainly for my personal Linux Mint setup.
Feel free to use and or modify

`deadbeef.h` needs to be added to the root directory of this project. Get it from here, or if you have the source already: https://github.com/DeaDBeeF-Player/deadbeef

To install, clone the repo and run
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install
```

Stats are kept in a local SQL Lite db
table name: zstat
| field name | type | description |
| id | file path | primary key |
| hash | blob | hash of part of the file, used for relinking a file if its location changes |
| playCount | int64 | number of times the track has been played |
| lastPlayed | int64 | epoch time of the last time the song finished playing |

This has only been tested on Linux Mint Cinnamon
Only tested on deadbeef 1.10.3