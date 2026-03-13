# FLUENT Language

**FLUENT** is a human-readable, naturally-expressive programming language that reads like plain English. Build scripts and automations with zero boilerplate.

Before running, you must run it via administrator, this is highly recommended and most features will not work without it.

Keep in mind, Its overhead is almost.. 0 always add an wait function for automated or just running any scripts to prevent from it bugging out due to its speed.
---

## Quick Install

### Linux / macOS
```bash
chmod +x install.sh
./install.sh
```

### Windows
```bat
install.bat
```

### Manual (Any Platform)
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
# Binary is at build/fluent (or build/Release/fluent.exe on Windows)
```

**Requirements:** CMake 3.16+, GCC 9+ / Clang 10+ / MSVC 2019+

---

## Running Scripts

```bash
fluent script.fluent
```

FLUENT watches your script for changes and **automatically reloads** the running instance when you save.

---

## Language Reference

### Comments
```fluent
:This is a comment:
```

### Variables
```fluent
let name be "Alice"
let age be 25
let items be a table containing "apple", "banana", "cherry"
let roll be a random number from 1 to 6
let secret be "password" obscured as "p########d"
```

### Displaying Output
```fluent
say "Hello, world!"
say "Name: ", name
say error "Something went wrong"
say warning "This feature is experimental"
say hardware_fluent
say hardwarecpu_fluent
say hardwaregpu_fluent
say hardwareram_fluent
say hardwarestorage_fluent
say hardwareos_fluent

paragraph "This is the first sentence.", continue "And this continues.", continue "And this."
```

### If Statements
```fluent
if age is above 18 then
    say "Adult"
otherwise
    say "Minor"
done

if name is a text then
    say "It is text"
done

if items is a table then
    say "It is a table"
done

if secret is obfuscated then
    say "Cannot read it"
done

if age is below 30 then
    say "Young"
done

if age is 25 then
    say "Exactly 25"
done

if name is "Alice" then
    say "Hi Alice!"
done
```

### Changing Variables
```fluent
change name to "Bob"
change age to 30
add 5 to age
subtract 3 to age
multiply 2 to age
divide 2 to age

:Tables:
add "grape" to items
remove "apple" from items

if items contains "banana" then
    say "Found banana!"
done
```

### Obfuscation
```fluent
obfuscate name and turn the obscured text to "****"
say name                          :prints ****:

set name to be deobfuscated
say "Revealed: ", name

say "Inline: ", make secret be deobfuscated
```

### Waiting
```fluent
wait for 5 seconds
wait for 2 minutes
wait for 1 hour

:Conditional wait:
wait for 5 seconds but if secret is obfuscated then change into 10 seconds

:Loop back:
wait for 10 seconds but if it reaches 5 seconds then repeat back to 10 seconds

:Switch duration:
wait for 10 seconds but if it reaches 9 seconds then make it be 15 seconds instead
```

### Loops
```fluent
loop up to 30 seconds while doing
    say "Looping!"
done

loop up to 5 minutes while changing score to 0 while doing
    say "Score reset"
done

loop only once while doing
    say "Runs once"
done
```

### Repeat (Variable Iteration)
```fluent
repeat counter while adding 10 stop only when counter is 100
repeat counter while reducing 5 stop only when counter is 0

repeat name if name has a text inside called "hello" and stop only when name does not have the text "hello"
```

### Math Helpers
```fluent
check age if its even if it is even then
    say "Even number"
otherwise
    say "Odd number"
done

check age if its odd if it is odd then
    say "Odd!"
done

check age if its a prime number if it is then
    say "Prime!"
done
```

### Ask (Input)
```fluent
ask "What is your name?" and have the options "Alice", "Bob", "Other"

ask "Continue?" and have the options "Yes", "No" if picked Yes then
    say "Great, continuing!"
done

ask "Continue?" and have the options "Yes", "No" if picked No then
    say "Stopping."
done
```

### Parallel Execution
```fluent
parallel
    say "Running simultaneously"
    say "This runs at the same time"
done
```

### When (Event Watchers)
```fluent
when age changes into 30 then
    say "Turned 30!"
otherwise
    say "Not yet"
done

when flag becomes true then
    say "Flag activated"
done
```

### Keeping Values / Files
```fluent
keep age in the system up to 5 hours
keep file called "data" in the same directory in the system up to 5 hours
keep name as "Alice" do not let it be changed for up to 1 hour
```

### Scheduling
```fluent
schedule at 10:00PM and when reached say "Time to sleep!"
schedule at 9:00AM and when reached say "Good morning!"
```

### Debugging / Logging
```fluent
say error "This will not work"
say warning "Deprecated feature"
log error name to errors.txt
```

### Files
```fluent
create a file called "Report" with the file type as txt then wait up to 2 seconds then delete
create a file called "Config" with the file type as txt then wait up to 1 second then rename to "Settings"
create a file called "App" with the file type as exe then wait up to 1 second then rename to "MyApp"
```

### Hardware
```fluent
say hardware_fluent
say hardwarecpu_fluent
say hardwaregpu_fluent
say hardwareram_fluent
say hardwarestorage_fluent
say hardwarevram_fluent
say hardwareos_fluent

check if hardware is connected to the internet if connected then
    say "Online!"
otherwise
    say "Offline"
done
```

### Imports & Modules
```fluent
import "mymodule"
say import_mymodule

say "Using module: ", import_mymodule
list import_mymodule
```

### Databases
```fluent
make a database containing "Alice", "Bob", "Charlie" and type be "Users"
list database_Users
```

### Globalization
```fluent
make name global
make name not global
```

### Privacy
```fluent
make apikey private and only public if authorized is true
make token private and only public if schedule is reached
make secret private and only public when system shutdown
```

### GUI Elements
```fluent
put button on gui1 and be called "Submit" and the position be 10 20 0 and color of the button be blue and background be white
put text on gui1 and text be "Welcome!" and the position be 5 5 0 and color of the text be black
put bar on gui1 and position be 0 50 0 and color be green
```

### Opening / Killing Applications
```fluent
open application called "notepad.exe"
open application called "bash" on path /usr/bin/

kill application called "notepad.exe"
kill application called "chrome.exe"
```

### System Commands
```fluent
system shutdown
system restart
```

### Lists
```fluent
list items
list database_Users
list import_mymodule
```

---

## Supported Types

| Type | Example |
|------|---------|
| Text | `"Hello"` |
| Number | `42`, `3.14` |
| Boolean | `true`, `false` |
| Table | `a table containing "a", "b"` |
| Obfuscated | `"hello" obscured as "h###o"` |
| Random | `a random number from 1 to 100` |

---

## Hot Reload

FLUENT automatically watches the running script. When you save changes, the interpreter restarts with the updated code immediately — no need to restart manually.

---

## License

MIT — Free to use, modify, and distribute.
