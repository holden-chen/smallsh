# Smallsh: A Simple Shell in C

## Introduction

Smallsh is a lightweight shell written in C that offers a command line interface similar to more complex shells, such as bash. It is designed as an educational project to demonstrate key concepts related to Unix process APIs, signal handling, and I/O redirection.

![smallsh-demo](https://github.com/holden-chen/smallsh/blob/main/smallsh_demo.gif)

## Features

- Interactive input prompt
- Tokenizes command line input
- Parameter expansion including:
    - '$$' for shell's process ID
    - '$?' for exit status of the last foreground command
    - '$!' for the process ID of the most recent background process
    - '${parameter}' for environment variables
- Built-in commands:
    - exit
    - cd
- Execution of non-built-in commands
- I/O redirection operators: <, >, >>
- Background command execution using &
- Custom signal handling for SIGINT and SIGTSTP

## How it Works

- Input: Manages background processes and reads a line of input from stdin or a script file.
- Word Splitting: Breaks the input line into individual words.
- Expansion: Performs variable and parameter expansion.
- Parsing: Converts words into syntactic tokens.
- Execution: Determines if the command is built-in and executes it. If not, it spawns a child process to run the command.
- Waiting: Handles the synchronization between the parent smallsh process and any child processes it spawns.

## Signal Handling

In interactive mode:

- SIGTSTP is ignored.
- SIGINT is ignored except when reading input.

## Usage

To compile the program, run ```make```. This will create an executable program named ```smallsh```. To run the program in interactive mode, use ```./smallsh``` in the same directory as the executable.
To run the program in non-interactive mode, use ```./smallsh < [script_name]```


## Learning Outcomes

By exploring and working with smallsh, you'll get hands-on experience with:

- The Unix process API
- Writing programs using Unix process API
- Understanding the concept of signals and their applications
- Writing programs utilizing the Unix signal handling API
- Understanding and implementing I/O redirection

## Final Thoughts

While smallsh does not aim to replace fully-fledged shells, it provides a simplified platform to understand and explore key Unix programming concepts. Feel free to dive into the source code and provide feedback or modifications!
