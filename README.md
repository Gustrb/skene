# Skene

From the classical greek, σκηνή, the *skene* was the structure at the back of a stage.
Analogus to the stage, this project is a toolchain of a computer environment.

## Main projects

They are all meant to be just a simple toolchain so I can play with different concepts as I want. This whole project
is not meant to have high-cohesion, but I want to re-use a lot of the tools I built to compose them and build cooler stuff.
Who knows, at the end we might have a full-fledged operating system.

- [] Testing utilities
    - [] TAP-producer header-only library
    - [] TAP-consumer tool to visualize test results
- [] ELF reader
- [] Assembler
- [] Linker
- [] C compiler
    - [] Bootstrapped compiler
- [] Shell
- [] UNIX-like tools

## Philosophy of the project

- *DON'T* use libraries if the problem is interesting enough to be solved;
- *DON'T* add complexity unless it is a complex program;
- *ALWAYS* use the strictest setting (compiler flags, linting rules, etc);
- *ALWAYS* test as much as possible to aid refactoring later;
- *NEVER* add code you don't understand;
- *ALWAYS* break the rules mentioned above (excluding this one to avoid paradoxes), if they don't make sense.

