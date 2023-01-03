# CustomShell
I implemented a "Simple Shell" (sish.c) that functions like a normal UNIX shell, including exit, cd, and history (limited to 100 entries) commands. The shell also supports piping of a large number of commands for user convenience. 

## History Details
history lists up to the last 100 stored commands.
history -c clears history (NOTE: "history" itself is counted in the list and so "history -c" will be the only entry after runnning "history -c")
history N will return the Nth index of history (i.e. history 5 will return the 5th index of history)
