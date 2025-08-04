
cmake_minimum_required(VERSION 3.16)
project(CliEbookReader CXX C)

set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Build Option ---
option(STATIC_BUILD "Build a self-contained, static executable" OFF)

if(STATIC_BUILD)
    message(STATUS "--- Performing a STATIC build ---")
    set(EXECUTABLE_NAME "ebook_reader_static")
    set(BUILD_SHARED_LIBS OFF)

    # --- Static Dependencies (FetchContent) ---
    FetchContent_Declare(ftxui_content GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git GIT_TAG v5.0.0)
    set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ftxui_content)

    FetchContent_Declare(libmobi_content GIT_REPOSITORY https://github.com/bfabiszewski/libmobi.git GIT_TAG v0.12)
    set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libmobi_content)
else()
    message(STATUS "--- Performing a DYNAMIC build ---")
    set(EXECUTABLE_NAME "ebook_reader")

    # --- Dynamic Dependencies (pkg-config) ---
    find_package(PkgConfig REQUIRED)
    find_package(Curses REQUIRED) # Find ncurses
    pkg_check_modules(FTXUI REQUIRED ftxui)
    link_directories(${FTXUI_LIBRARY_DIRS})
    pkg_check_modules(MOBI REQUIRED libmobi)
    link_directories(${MOBI_LIBRARY_DIRS})
endif()

# --- Common Dependencies (Always fetched) ---
find_package(ZLIB REQUIRED)
FetchContent_Declare(libzip_content GIT_REPOSITORY https://github.com/nih-at/libzip.git GIT_TAG v1.10.1)
set(ENABLE_TOOLS OFF CACHE BOOL "" FORCE)
set(ENABLE_REGRESS OFF CACHE BOOL "" FORCE)
set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(ENABLE_BZIP2 OFF CACHE BOOL "" FORCE)
set(ENABLE_LZMA OFF CACHE BOOL "" FORCE)
set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
set(ENABLE_GNUTLS OFF CACHE BOOL "" FORCE)
set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENSSL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(libzip_content)

FetchContent_Declare(gumbo_content GIT_REPOSITORY https://github.com/google/gumbo-parser.git GIT_TAG v0.10.1)
FetchContent_MakeAvailable(gumbo_content)
add_library(gumbo STATIC
    ${gumbo_content_SOURCE_DIR}/src/attribute.c
    ${gumbo_content_SOURCE_DIR}/src/char_ref.c
    ${gumbo_content_SOURCE_DIR}/src/error.c
    ${gumbo_content_SOURCE_DIR}/src/parser.c
    ${gumbo_content_SOURCE_DIR}/src/string_buffer.c
    ${gumbo_content_SOURCE_DIR}/src/string_piece.c
    ${gumbo_content_SOURCE_DIR}/src/tag.c
    ${gumbo_content_SOURCE_DIR}/src/tokenizer.c
    ${gumbo_content_SOURCE_DIR}/src/utf8.c
    ${gumbo_content_SOURCE_DIR}/src/util.c
    ${gumbo_content_SOURCE_DIR}/src/vector.c
)
target_include_directories(gumbo PUBLIC ${gumbo_content_SOURCE_DIR}/src)

FetchContent_Declare(tinyxml2 GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git GIT_TAG 10.0.0)
FetchContent_MakeAvailable(tinyxml2)

FetchContent_Declare(sqlite3_content URL https://www.sqlite.org/2023/sqlite-amalgamation-3420000.zip)
FetchContent_MakeAvailable(sqlite3_content)
add_library(sqlite3_lib STATIC 
    ${sqlite3_content_SOURCE_DIR}/sqlite3.c
)
target_include_directories(sqlite3_lib PUBLIC 
    ${sqlite3_content_SOURCE_DIR}
)

# --- Source Files ---
add_executable(${EXECUTABLE_NAME}
    src/main.cpp
    src/EpubParser.cpp
    src/HtmlRenderer.cpp
    src/BookViewModel.cpp
    src/DebugLogger.cpp
    src/TxtParser.cpp
    src/LibraryManager.cpp
    src/DatabaseManager.cpp
    src/MobiParser.cpp
)

# --- Include Directories ---
target_include_directories(${EXECUTABLE_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)
if(NOT STATIC_BUILD)
    target_include_directories(${EXECUTABLE_NAME} PUBLIC 
        ${MOBI_INCLUDE_DIRS}
        ${FTXUI_INCLUDE_DIRS}
    )
endif()

# --- Linking Libraries ---
target_link_libraries(${EXECUTABLE_NAME}
  PRIVATE
  ZLIB::ZLIB
  zip
  gumbo
  tinyxml2
  sqlite3_lib
)
if(STATIC_BUILD)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ftxui::screen ftxui::dom ftxui::component mobi)
else()
    # Be explicit for dynamic builds as well, as ${FTXUI_LIBRARIES} can be inconsistent.
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ftxui-screen ftxui-dom ftxui-component ${MOBI_LIBRARIES} ${CURSES_LIBRARIES})
endif()
