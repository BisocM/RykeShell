---

# **RykeShell**

![Banner](https://github.com/BisocM/RykeShell/blob/master/RykeShell_Rectangle_Transparent.png?raw=true)

[![CodeQL Analysis](https://github.com/BisocM/RykeShell/actions/workflows/codeql.yml/badge.svg)](https://github.com/BisocM/RykeShell/actions/workflows/codeql.yml)

---

## **Introduction**

**RykeShell** is a modern, custom Unix shell implemented in C++. It combines essential shell functionalities with advanced features to provide a user-friendly and efficient command-line environment. RykeShell supports enhanced command parsing, case-insensitive auto-completion with correct casing, built-in commands, customizable theming, and more.

---

## **Features**

- **Customizable Prompt**: Displays the username, hostname, and current directory with color customization using the `theme` command.

- **Advanced Command Parsing**: Supports piping (`|`), input/output redirection (`>`, `<`, `>>`), background execution (`&`), and command chaining (`&&`, `||`).
- **Modern Redirections**: `|&`, `&>`, `2>`, `2>>`, here-documents (`<<`) and here-strings (`<<<`).
- **Scripting Mode**: Run `./RykeShell script.ryk` to execute scripts with the same engine as interactive mode.

- **Built-in Commands**:
    - `cd`: Change the current directory.
    - `pwd`: Display the current working directory.
    - `history`: View the command history.
    - `alias`: Create command aliases.
    - `prompt`: Configure the prompt template (supports `{user}`, `{host}`, `{cwd}`, `{color}`, `{cwdcolor}`, `{reset}`).
    - `theme`: Change the prompt color.
    - `set`: Toggle shell options (`-e`, `-u`, `-x`, `-C`, `-m`, `notify`, `history-ignore-dups`, `noclobber`, etc.).
    - `jobs`, `jobs -l`, `fg`, `bg`, `disown` (via `bg` + `set -m`): Job control for background tasks.
    - `source`: Load and run another script in the current session.
    - `plugin load <path>`: Dynamically load a plugin that exposes `register_plugin(ryke::Shell&)`.
    - `exit`: Exit RykeShell.
    - `help`: Display help information for built-in commands.

- **Wildcard Expansion**: Supports glob patterns (`*`, `?`) for file and directory matching.

- **Environment Variable Expansion**: Expands variables using `$VAR` and `${VAR}`, including default values with `${VAR:-default}`; respects `set -u` for unset vars.
- **Brace/Arithmetic/Command Substitution**: `{a,b}`/`{1..3}`, `$((1+2))`, and `$(cmd)` all work.

- **Persistent State**: History, aliases, prompt template, and prompt color are stored under your home directory for the next session.

- **Enhanced Auto-Completion**:
    - **Case-Insensitive Matching**: Type commands and filenames without worrying about case sensitivity.
    - **Correct Casing in Suggestions**: Auto-completed suggestions use the correct casing as they exist in the filesystem or system commands.
    - **Inline Suggestions**: Provides suggestions as you type, with unmatched characters displayed in a different color.
    - **History search**: Ctrl+R for reverse incremental search.

- **Signal Handling**: Safely handles `SIGINT` (Ctrl+C) to prevent unintended termination.

- **Multiline Support**: Allows for multiline commands.

---

## **Getting Started**

### **Prerequisites**

- **C++20-Compatible Compiler**: RykeShell requires a compiler that supports C++20 features (e.g., `g++` version 10 or higher).

- **C++ Standard Library and POSIX-Compliant System**: Unix/Linux environment is required.

### **Building RykeShell**

#### **Clone the Repository**

```bash
git clone https://github.com/BisocM/RykeShell.git
cd RykeShell
```

#### **Build with CMake**

RykeShell uses CMake for building. Ensure you have `cmake` installed.

```bash
mkdir build
cd build
cmake ..
make

# Run tests
ctest
```

#### **Alternatively, Build Manually**

If you prefer to compile directly with `g++`, ensure you compile with the `-std=c++20` flag and include the necessary libraries.

##### **Step 1: Navigate to the Source Directory**

```bash
cd src
```

##### **Step 2: Compile with `g++`**

```bash
g++ -Wall -Wextra -Wpedantic -std=c++20 -I../include -o RykeShell \
    main.cpp ryke_shell.cpp utils.cpp input.cpp autocomplete.cpp parser.cpp executor.cpp commands.cpp -ldl
```

**Note:** Replace `g++` with `g++-10` or higher if necessary.

---

## **Running RykeShell**

After building, you can run RykeShell using:

```bash
./RykeShell
```

Run a script file:

```bash
./RykeShell path/to/script.ryk
```

---

## **Usage**

- **Executing Commands**: Type commands as you would in a standard shell.

  ```bash
  ls -l
  ```

- **Auto-Completion**:

    - **Inline Suggestions**: As you type, RykeShell provides inline suggestions for commands and filenames.
    - **Accepting Suggestions**: Press `Tab` to accept the suggestion.
    - **Case-Insensitive Matching**: Type commands and filenames without worrying about case; the shell matches them case-insensitively but preserves the correct casing.

    **Example:**

    - If you have a file named `Makefile`, typing `cat ma` will show an inline suggestion for `Makefile`, and pressing `Tab` will complete it to `cat Makefile`.

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

    - **Set Prompt Template**

      ```bash
      prompt "{color}{user}@{host}{reset}:{cwdcolor}{cwd}{reset}$ "
      ```

    - **Toggle Options (`set`)**

      ```bash
      set -e      # exit on error
      set -u      # error on unset vars
      set -x      # trace commands
      set -C      # noclobber
      set -m      # monitor job control
      set -o notify
      ```

    - **Source a Script**

      ```bash
      source ~/.rykeshellrc
      ```

    - **Load a Plugin**

      ```bash
      plugin load /path/to/plugin.so
      ```

    - **Job Control**

      ```bash
      sleep 5 &
      jobs
      fg        # bring most recent job to the foreground
      bg 1      # resume job 1 in the background
      ```

    - **Exit RykeShell**

      ```bash
      exit
      ```

    - **Display Help**

      ```bash
      help
      ```

- **Advanced Command Features**:

    - **Piping**

      ```bash
      ls | grep txt
      ```

    - **Redirection**

        - **Output Redirection:**

          ```bash
          ls > output.txt
          ```

        - **Input Redirection:**

          ```bash
          sort < unsorted.txt
          ```

        - **Append Output:**

          ```bash
          echo "New Line" >> file.txt
          ```

        - **Redirect stderr:**

          ```bash
          cmd 2> errors.log
          cmd &> all.log
          cmd |& tee both.log
          ```

        - **Here-doc / Here-string:**

          ```bash
          cat <<EOF
          hello
          EOF

          cat <<< "inline content"
          ```

    - **Background Execution**

      ```bash
      long_running_command &
      ```

    - **Command Chaining**

        - **AND Operator (`&&`):**

          Execute the next command only if the previous command succeeds.

          ```bash
          mkdir new_dir && cd new_dir
          ```

        - **OR Operator (`||`):**

          Execute the next command only if the previous command fails.

          ```bash
          cd existing_dir || echo "Directory does not exist."
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

- **State Files**

  RykeShell persists session data in your home directory by default:

  - `~/.rykeshell_history`
  - `~/.rykeshell_aliases`
  - `~/.rykeshell_config` (prompt, options)
  - `~/.rykeshellrc` (sourced at startup if present)

---

## **Contributing**

We welcome contributions to RykeShell! Whether you're fixing bugs, adding new features, or improving documentation, your help is appreciated.

### **Issue Reporting**

If you encounter any issues or have feature requests, please submit an issue on GitHub. We have issue templates to guide you:

- **Bug Report**
- **Feature Request**
- **Question**

When submitting an issue, please provide as much detail as possible to help us address it effectively.

### **Pull Requests**

To contribute code:

1. **Fork the Repository**
2. **Create a Feature Branch**

   ```bash
   git checkout -b feature/your-feature-name
   ```

3. **Commit Your Changes**

   ```bash
   git commit -am 'Add new feature'
   ```

4. **Push to Your Fork**

   ```bash
   git push origin feature/your-feature-name
   ```

5. **Submit a Pull Request**

---

## **License**

RykeShell is released under the [MIT License](LICENSE).

---

## **Acknowledgments**

- Inspired by traditional Unix shells and built with modern C++ features.
- Thanks to all contributors who have helped improve RykeShell.

---

## **Contact**

For questions or suggestions, feel free to open an issue or reach out to the maintainer.

---

## **Note on Compilation**

RykeShell makes use of C++20 features. Ensure your compiler supports C++20. You can check your `g++` version:

```bash
g++ --version
```

If necessary, install a newer version of `g++`.

### **Example Installation on Ubuntu**

```bash
sudo apt-get update
sudo apt-get install g++-10
```

Then, compile using `g++-10`:

```bash
g++-10 -Wall -Wextra -std=c++20 -g -I../include -o RykeShell main.cpp utils.cpp input.cpp autocomplete.cpp parser.cpp executor.cpp commands.cpp -lncurses
```

---
