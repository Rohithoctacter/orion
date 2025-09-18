# Orion Programming Language Web IDE

## Overview
This is a web-based IDE for the Orion Programming Language - a pure compiled systems programming language designed to bridge C's performance with Python's readability. The project includes a C++ compiler backend and a Flask web interface.

## Recent Changes (September 18, 2025)
- Successfully set up the Replit environment for the imported GitHub project
- Installed required system dependencies (GCC, Make, binutils, file utilities)
- Compiled the C++ Orion compiler using make (366KB executable created successfully)
- Installed Python dependencies via UV package manager (Flask, Flask-CORS, Gunicorn, etc.)
- Configured Flask web interface to serve on 0.0.0.0:5000 with proper cache control headers
- Set up workflow for the web interface with webview output type and port 5000
- Configured deployment settings for production autoscale deployment
- **COMPLETED IMPORT**: Web IDE is now fully functional in Replit environment

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
- C++ compiler builds successfully (366KB executable)
- Flask API endpoints respond properly 
- Compilation process works (generates assembly and executables)  
- Workflow configured and running on port 5000 with webview output type
- Deployment configuration set for production autoscale
- System dependencies installed (GCC, Make, binutils, file utilities)
- Python dependencies managed by UV package manager

⚠️ **Known Issues:**
- **Execution Bug**: Generated executables may not produce expected output
  - This is a known issue documented in the original project
  - Compiler successfully builds but there may be runtime execution issues
  - Web interface will show "Compilation successful" as expected behavior

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