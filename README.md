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

- **Built-in Commands**:
    - `cd`: Change the current directory.
    - `pwd`: Display the current working directory.
    - `history`: View the command history.
    - `alias`: Create command aliases.
    - `theme`: Change the prompt color.
    - `exit`: Exit RykeShell.
    - `help`: Display help information for built-in commands.

- **Wildcard Expansion**: Supports glob patterns (`*`, `?`) for file and directory matching.

- **Environment Variable Expansion**: Expands variables using `$VAR` and `${VAR}`, including default values with `${VAR:-default}`.

- **Enhanced Auto-Completion**:
    - **Case-Insensitive Matching**: Type commands and filenames without worrying about case sensitivity.
    - **Correct Casing in Suggestions**: Auto-completed suggestions use the correct casing as they exist in the filesystem or system commands.
    - **Inline Suggestions**: Provides suggestions as you type, with unmatched characters displayed in a different color.

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
```

#### **Alternatively, Build Manually**

If you prefer to compile directly with `g++`, ensure you compile with the `-std=c++20` flag and include the necessary libraries.

##### **Step 1: Navigate to the Source Directory**

```bash
cd src
```

##### **Step 2: Compile with `g++`**

```bash
g++ -Wall -Wextra -std=c++20 -g -I../include -o RykeShell main.cpp utils.cpp input.cpp autocomplete.cpp parser.cpp executor.cpp commands.cpp -lncurses
```

**Note:** Replace `g++` with `g++-10` or higher if necessary.

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
