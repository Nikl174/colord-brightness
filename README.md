# ICC Color Brightness
use ICC color profiles to change the brightness of the display

# workarround for icc file
- for a icc profile (which is needed for a setting it systemwide) a file need to be saved someware -> read/write in the slow SSD 
- as a work arround, 2 solutions:
    1. save the file in a tmpfs
    2. use a named pipe as a file, eg.:
    ```bash
    mkfifo my_pipe
    cat file > my_pipe
    echo my_pipe > new_file
    rm my_pipe
    ```
