# All Reader - A Command-Line Ebook Reader

All Reader is a versatile, cross-platform ebook reader designed for the terminal. It provides a clean, distraction-free reading experience with a focus on performance and usability.

## Core Features

- **Multi-Format Support:** Reads EPUB, MOBI, AZW3, and plain TXT files.
- **Intelligent Table of Contents:** Automatically extracts real chapter titles from MOBI/AZW3 files and supports nested TOC navigation.
- **Smart Pagination:** Ensures every chapter begins on a new page for a natural reading flow.
- **Persistent Library:** Manages your book collection in a local database (`~/.all_reader/`).
- **Paginated Views:** Both the library and the table of contents are paginated for easy navigation of large collections.
- **Convenience:** Remembers the last directory you used to add books, simplifying the process of importing new files.

## Dependencies

To run the pre-compiled binary or to compile the project from source, you will need to have the following libraries installed on your system.

### macOS (via [Homebrew](https://brew.sh/))

- **FTXUI:** For the terminal-based user interface.
- **libmobi:** For parsing MOBI and AZW3 files.

### Linux

- **FTXUI:** `libftxui-dev` (Debian/Ubuntu), `ftxui-devel` (Fedora), `ftxui` (Arch AUR).
- **libmobi:** `libmobi-dev` (Debian/Ubuntu), `libmobi-devel` (Fedora), `libmobi` (Arch).

### Internal Dependencies (Handled Automatically)

The following libraries are also dependencies, but they are managed automatically by CMake's `FetchContent` module. You **do not** need to install them manually.

- **libzip:** For handling EPUB container files.
- **gumbo-parser:** For parsing HTML content within ebooks.
- **TinyXML2:** For parsing XML files (e.g., `container.xml` in EPUBs).
- **SQLite3:** For the book library database.

---

## How to Compile (Dynamic Build)

### macOS

1.  **Install Dependencies:**
    ```bash
    brew install cmake pkg-config ftxui libmobi
    ```

2.  **Configure and Compile:**
    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```
    The executable `ebook_reader` will be created in the `build` directory.

### Linux

1.  **Install Dependencies:**

    **For Debian/Ubuntu:**
    ```bash
    sudo apt update
    sudo apt install build-essential cmake pkg-config libftxui-dev libmobi-dev
    ```

    **For Fedora/RHEL/CentOS:**
    ```bash
    sudo dnf groupinstall "Development Tools"
    sudo dnf install cmake pkg-config ftxui-devel libmobi-devel
    ```

    **For Arch Linux (using an AUR helper like `yay`):**
    ```bash
    yay -S base-devel cmake pkg-config libmobi ftxui
    ```

2.  **Configure and Compile:**
    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```
    The executable `ebook_reader` will be created in the `build` directory.
