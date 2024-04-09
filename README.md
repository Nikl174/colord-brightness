# ICC Color Brightness
use ICC color profiles to change the brightness of the display

## workarround for icc file
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

## Filewatcher approach
-   use inotify and conditional variables as notification system for file changes
