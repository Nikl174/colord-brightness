# ICC Color Brightness WIP
use ICC color profiles to change the brightness of the display
inspired by [icc-brightness](https://github.com/udifuchs/icc-brightness) but more performant and does not produces ICC files in local directory

## Dependencies
```bash
colord
lcms2
easyloggingpp
# only for building
pkgconf
cmake
```

## How it works
- uses [inotify](https://man7.org/linux/man-pages/man7/inotify.7.html) to watch for changes on the kernel backlight driver (currently only /sys/class/backlight/intel_backlight/brightness)
- if this file gets modified, it sends events and the changed content to the main programm
- the main programm then uses [little-cms](https://github.com/mm2/Little-CMS) to create a color profile with the brightness from the file
- this profile then gets applied to the [colord-daemon](https://github.com/hughsie/colord) but the daemon needs a file to read from
- this file is a file only in memory, and only exist as long as the programm runs (it uses the syscall [memfd_create](https://www.man7.org/linux/man-pages/man2/memfd_create.2.html#top_of_page))

## Where it works
- should work on all linux-based systems using the colord-daemon for color management **and a compositor (wayland or Xorg) which supports color-manaement** (e.g. Gnome, KDE, etc)
### Currently testet on
- Archlinux with Gnome
### Compositor not working with
- Sway (no color-manaement supports, [but could be in future](https://github.com/swaywm/sway/pull/5586))

## Development

### Todo
- add commandline interface
- add config file?
- add AUR package
- add systemd-user-script
- improve structure
- improve readme

### workarround for icc file
- for a icc profile (which is needed for a setting it system wide) a file need to be saved some ware -> read/write in the slow SSD 
- as a work arround, 2 solutions:
    1. save the file in a tmpfs
    2. use a named pipe as a file, eg.:
    ```bash
    mkfifo my_pipe
    cat file > my_pipe
    echo my_pipe > new_file
    rm my_pipe
    ```
    3. (current approach) use a memfd (memfd_create) to simulate a file in a fd which is saved in ram an cleared, if reference is cleared

### Filewatcher approach
-   use inotify and conditional variables as notification system for file changes
