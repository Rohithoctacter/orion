# Orion Programming Language Web IDE

## Overview
This is a web-based IDE for the Orion Programming Language - a pure compiled systems programming language designed to bridge C's performance with Python's readability. The project includes a C++ compiler backend and a Flask web interface.

## Recent Changes (September 18, 2025)
- Successfully set up the Replit environment for the imported GitHub project
- Compiled the C++ Orion compiler using make
- Installed Python dependencies (Flask, Flask-CORS, Gunicorn, etc.)
- Configured Flask web interface to serve on 0.0.0.0:5000 with proper cache control
- Set up workflow for the web interface with webview output type
- Configured deployment settings for production autoscale

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
- Web interface loads and displays correctly
- C++ compiler builds successfully 
- Flask API endpoints respond properly
- Compilation process works (generates assembly and executables)
- Workflow configured and running on port 5000
- Deployment configuration set for production

⚠️ **Known Issues:**
- **Critical Bug**: Generated executables don't produce output
  - Root cause: The `main` function in generated assembly doesn't call the user-defined `fn main()` function
  - This is a code generation bug in the C++ compiler
  - Web interface shows "Compilation successful" instead of program output

## Build System
- Uses `make` for C++ compiler compilation
- Uses `uv` for Python dependency management
- Runtime dependencies: GCC, binutils for assembly and linking

## User Preferences
- Maintain existing code structure and conventions
- Keep the educational/demo nature of the language
- Preserve clean Python-like syntax design

## Development Notes
The Orion language is designed as a learning/demonstration project. The core concept of "C performance with Python readability" is implemented, but there are execution bugs that need to be addressed in the code generator.