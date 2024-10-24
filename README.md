# **RykeShell**

![Banner](https://github.com/BisocM/RykeShell/blob/master/RykeShell_Rectangle_Transparent.png?raw=true)

[![CodeQL Analysis](https://github.com/BisocM/RykeShell/actions/workflows/codeql.yml/badge.svg)](https://github.com/BisocM/RykeShell/actions/workflows/codeql.yml)


## **Introduction**

**RykeShell** is a custom Unix shell implemented in C++. It combines essential shell functionalities with modern features to provide a user-friendly and efficient command-line environment. RykeShell supports advanced command parsing, wildcard expansion, built-in commands, customizable theming, and basic auto-completion.

---

## **Features**

- **Customizable Prompt**: Displays the username, hostname, and current directory with color customization using the `theme` command.
- **Advanced Command Parsing**: Supports piping (`|`), input/output redirection (`>`, `<`, `>>`), and background execution (`&`).
- **Built-in Commands**:
    - `cd`: Change the current directory.
    - `pwd`: Display the current working directory.
    - `history`: View the command history.
    - `alias`: Create command aliases.
    - `theme`: Change the prompt color.
- **Wildcard Expansion**: Supports glob patterns (`*`, `?`) for file and directory matching.
- **Environment Variable Expansion**: Expands variables using `$VAR` and `${VAR}`, including default values with `${VAR:-default}`.
- **Basic Auto-Completion**: Provides auto-completion for commands and filenames when pressing the `Tab` key.
- **Signal Handling**: Safely handles `SIGINT` (Ctrl+C) to prevent unintended termination.
- **Multiline Support**: Allows for multiline commands.

---

## **Getting Started**

### **Prerequisites**

- A C++11-compatible compiler (e.g., `g++`).
- C++ Standard Library and POSIX-compliant system (Unix/Linux environment).

### **Building RykeShell**

1. **Clone the Repository**

   ```bash
   git clone https://github.com/bisocm/RykeShell.git
   cd RykeShell
   ```

2. **Build with CMake**

   RykeShell uses CMake for building. Ensure you have `cmake` installed.

   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

3. **Alternatively, Build Manually**

   If you prefer to compile directly with `g++`:

   ```bash
   g++ -std=c++11 -o RykeShell main.cpp parser.cpp executor.cpp utils.cpp input.cpp
   ```

---

## **Running RykeShell**

After building, you can run RykeShell using:

```bash
./RykeShell
```

---

## **Usage**

- **Executing Commands**: Type commands as you would in a standard shell.

  ```bash
  ls -l
  ```

- **Auto-Completion**: Press `Tab` to auto-complete commands and filenames.

- **Built-in Commands**:

    - **Change Directory**

      ```bash
      cd /path/to/directory
      ```

    - **Print Working Directory**

      ```bash
      pwd
      ```

    - **View Command History**

      ```bash
      history
      ```

    - **Set Alias**

      ```bash
      alias ll='ls -l'
      ```

    - **Change Prompt Theme**

      ```bash
      theme blue
      ```

- **Advanced Command Features**:

    - **Piping**

      ```bash
      ls | grep txt
      ```

    - **Redirection**

        - Output Redirection:

          ```bash
          ls > output.txt
          ```

        - Input Redirection:

          ```bash
          sort < unsorted.txt
          ```

        - Append Output:

          ```bash
          echo "New Line" >> file.txt
          ```

    - **Background Execution**

      ```bash
      long_running_command &
      ```

- **Wildcard Expansion**

  ```bash
  ls *.cpp
  ```

- **Environment Variable Expansion**

  ```bash
  echo $HOME
  echo ${USER}
  echo ${UNSET_VAR:-default_value}
  ```

---

## **Customization**

- **Changing Prompt Color**

  Use the `theme` command with one of the available colors:

    - `red`
    - `green`
    - `yellow`
    - `blue`
    - `magenta`
    - `cyan`

  Example:

  ```bash
  theme magenta
  ```

- **Creating Aliases**

  Create shortcuts for common commands:

  ```bash
  alias gs='git status'
  ```

---

## **License**

RykeShell is released under the [MIT License](LICENSE).