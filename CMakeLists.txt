
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
    
    # Find FTXUI with fallback options
    pkg_check_modules(FTXUI ftxui)
    if(FTXUI_FOUND)
        message(STATUS "FTXUI found via pkg-config")
        message(STATUS "  FTXUI_LIBRARIES: ${FTXUI_LIBRARIES}")
        message(STATUS "  FTXUI_LIBRARY_DIRS: ${FTXUI_LIBRARY_DIRS}")
        message(STATUS "  FTXUI_INCLUDE_DIRS: ${FTXUI_INCLUDE_DIRS}")
        link_directories(${FTXUI_LIBRARY_DIRS})
        set(FTXUI_METHOD "pkg-config")
    else()
        # Fallback: try to find FTXUI manually
        message(STATUS "FTXUI not found via pkg-config, trying manual detection...")
        
        find_library(FTXUI_SCREEN_LIB NAMES ftxui-screen)
        find_library(FTXUI_DOM_LIB NAMES ftxui-dom)  
        find_library(FTXUI_COMPONENT_LIB NAMES ftxui-component)
        find_path(FTXUI_INCLUDE_PATH ftxui/screen/screen.hpp)
        
        if(FTXUI_SCREEN_LIB AND FTXUI_DOM_LIB AND FTXUI_COMPONENT_LIB AND FTXUI_INCLUDE_PATH)
            message(STATUS "FTXUI found manually")
            message(STATUS "  Screen lib: ${FTXUI_SCREEN_LIB}")
            message(STATUS "  DOM lib: ${FTXUI_DOM_LIB}")
            message(STATUS "  Component lib: ${FTXUI_COMPONENT_LIB}")
            message(STATUS "  Include path: ${FTXUI_INCLUDE_PATH}")
            
            set(FTXUI_LIBRARIES "${FTXUI_COMPONENT_LIB};${FTXUI_DOM_LIB};${FTXUI_SCREEN_LIB}")
            set(FTXUI_INCLUDE_DIRS "${FTXUI_INCLUDE_PATH}")
            set(FTXUI_METHOD "manual")
        else()
            message(FATAL_ERROR "FTXUI not found. Please install libftxui-dev or build from source.")
        endif()
    endif()
    
    # Find libmobi with detailed error reporting  
    pkg_check_modules(MOBI REQUIRED libmobi)
    if(MOBI_FOUND)
        message(STATUS "libmobi found via pkg-config")
        message(STATUS "  MOBI_LIBRARIES: ${MOBI_LIBRARIES}")
        message(STATUS "  MOBI_LIBRARY_DIRS: ${MOBI_LIBRARY_DIRS}")
        message(STATUS "  MOBI_INCLUDE_DIRS: ${MOBI_INCLUDE_DIRS}")
        link_directories(${MOBI_LIBRARY_DIRS})
    else()
        message(FATAL_ERROR "libmobi not found via pkg-config. Install libmobi-dev or similar package.")
    endif()
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
    # Dynamic build: use discovered libraries
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${FTXUI_LIBRARIES} ${MOBI_LIBRARIES} ${CURSES_LIBRARIES})
    
    # Add compiler flags from pkg-config if available
    if(FTXUI_METHOD STREQUAL "pkg-config")
        target_compile_options(${EXECUTABLE_NAME} PRIVATE ${FTXUI_CFLAGS_OTHER})
    endif()
    if(MOBI_FOUND)
        target_compile_options(${EXECUTABLE_NAME} PRIVATE ${MOBI_CFLAGS_OTHER})
    endif()
    
    message(STATUS "Dynamic build configuration:")
    message(STATUS "  FTXUI method: ${FTXUI_METHOD}")
    message(STATUS "  FTXUI libraries: ${FTXUI_LIBRARIES}")
    message(STATUS "  MOBI libraries: ${MOBI_LIBRARIES}")
endif()
