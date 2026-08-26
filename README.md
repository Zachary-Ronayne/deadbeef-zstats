# Intro

This is a plugin for deadbeef to track play count and last played time.

Made mainly for my personal Linux Mint setup.

Feel free to use and or modify

# Installing

You'll need to build the plugin from this repo. Clone it locally and build.

`deadbeef.h` needs to be added to the root directory of this project. Get it from here, or if you have the source already: https://github.com/DeaDBeeF-Player/deadbeef

Requires that sql lite 3 is installed

For Linux Mint:
`sudo apt install libsqlite3-dev`

To install, clone the repo and run
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install
```
Or run `install.sh`

To uninstall, delete the plugin at `~/.local/lib/deadbeef/zstat.so` or run `uninstall.sh` and restart deadbeef

When installed, `zstat.so` should be in that deadbeef directory

# Fields

Stats are stored and displayed in deadbeef like normal metadata fields

`%zstat_play_count%` is the number of times the track has been played

`%zstat_last_played_epoch%` is the epoch time, in seconds, when the track was played. Use for sorting if the chosen last played string format isn't directly sortable

`%zstat_last_played%` is a human readable string timestamp of the last time the track was played. Not the source of truth, always overridden by the epoch timestamp

# DB

Stats are kept in a local SQLite db. This is to avoid dealing with storing data on different audio file types.
One quirk of this is that if you edit the metadata in deadbeef, it will not persist to the local db.
If you want to reset the stats, delete the local db file or manually update the db, then restart deadbeef.

The db file is stored in: `~/deadbeef/zstat/zstat.db`

table name: `zstat`
| field name | type | description |
| - | - | - |
| `path` | file path | primary key |
| `hash` | blob | hash of part of the file, used for relinking a file if its location changes |
| `play_count` | int64 | number of times the track has been played |
| `last_played` | int64 | epoch time of the last time the song finished playing |

`hash` is not currently implemented

# Notes

This has only been tested on Linux Mint Cinnamon

Only tested on deadbeef 1.10.3