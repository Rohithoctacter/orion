# Orion Programming Language Web IDE

## Overview
This is a web-based IDE for the Orion Programming Language - a pure compiled systems programming language designed to bridge C's performance with Python's readability. The project includes a C++ compiler backend and a Flask web interface.

## Recent Changes (September 19, 2025)
- **FIXED EXECUTION BUG**: Added missing `print` function to runtime.c to resolve linking errors
- Successfully rebuilt C++ compiler with working print functionality
- Verified web interface works correctly with compilation and execution
- Updated workflow configuration with proper webview output type
- Confirmed deployment configuration for production autoscale
- **COMPLETED IMPORT**: Web IDE is now fully functional in Replit environment with working compilation
- **NEW FEATURE**: Implemented Python-style dictionary assignment operations (dict["key"] = value)
  - Added type-aware collection assignment functions to runtime.c
  - Enhanced IndexAssignment code generator to support both lists and dictionaries
  - Fixed type inference and print conversion for collection access
  - Dictionary and list assignment operations now work without segmentation faults

## Project Architecture
### Backend Components
- **C++ Compiler** (`compiler/` directory): Core Orion language compiler
  - Lexer, Parser, Type Checker, and Code Generator
  - Compiles Orion code to x86-64 assembly and then to native executables
  - Runtime library for memory management and built-in functions
- **Flask Web Server** (`app.py`, `main.py`): HTTP API for compilation requests
  - `/compile` endpoint: Compiles and executes Orion code
  - `/check-syntax` endpoint: Syntax validation
  - Static file serving with cache control

### Frontend Components
- **Web Interface** (`index.html`, `style.css`, `script.js`): Interactive code editor
  - Bootstrap 5 and Prism.js for syntax highlighting
  - Real-time compilation and execution
  - Example code loading and interactive input support

## Current Status
✅ **Working:**
- Web interface loads and displays correctly with Bootstrap 5 styling
- C++ compiler builds successfully with working `print` function
- Flask API endpoints respond properly with actual program output
- Compilation and execution works correctly (generates assembly, executables, and runs them)
- Workflow configured and running on port 5000 with webview output type
- Deployment configuration set for production autoscale
- System dependencies installed (GCC, Make, binutils, file utilities)
- Python dependencies managed by UV package manager
- Runtime linking resolved - programs now execute and produce correct output
- **Dictionary Operations**: Python-style assignment (dict["key"] = value) for adding/updating entries
- **List Operations**: Array assignment (list[index] = value) works correctly

✅ **Fixed Issues:**
- **RESOLVED**: Execution bug was caused by missing `print` function in runtime.c
- **RESOLVED**: Linking errors resolved by adding proper `print` function implementation
- **VERIFIED**: Web interface now shows actual program output, not just compilation success

## Build System
- Uses `make` for C++ compiler compilation
- Uses `uv` for Python dependency management
- Runtime dependencies: GCC, binutils for assembly and linking

## User Preferences
- Maintain existing code structure and conventions
- Keep the educational/demo nature of the language
- Preserve clean Python-like syntax design

## Development Notes
The Orion language is designed as a learning/demonstration project. The core concept of "C performance with Python readability" is implemented with working dictionary and list assignment operations.

**Known Limitation**: String values retrieved from dictionaries print as numeric addresses instead of string content due to the runtime's type-agnostic storage system. Assignment and retrieval operations work correctly; the limitation only affects string display.