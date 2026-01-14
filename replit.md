# Orion Programming Language Web IDE

## Overview
This project is a web-based IDE for Orion, a pure compiled systems programming language. Orion aims to combine C's performance with Python's readability. The platform features a Flask web server, a native C++ compiler, and a responsive web frontend, enabling users to write, compile, and execute Orion code directly in the browser. 

**Current Status**: Successfully imported and configured for Replit environment. All systems fully operational - compiler, web interface, and API endpoints working perfectly. Ready for production deployment.

## User Preferences
Preferred communication style: Simple, everyday language.

## Recent Changes (October 13, 2025)
- **Dictionary Implementation**: Added complete Python-style dictionary support with {key: value} syntax
  - Implemented hash table backend in C with reference counting
  - Added runtime functions: dict_new, dict_get, dict_set, dict_delete, dict_pop, dict_keys, dict_values, dict_items, dict_clear, dict_update
  - Extended parser to support dictionary literals and operations
  - Added DICT type to type system with proper type inference
  - Code generator produces x86-64 assembly for dictionary operations
  - **Tested**: Dictionary creation, access, and basic operations working correctly
- **Memory Management Implementation**: Added reference counting for heap-allocated types (lists, strings, ranges, dictionaries)
  - Implemented retain/release functions in runtime.c with refcount tracking
  - Added automatic cleanup when variables go out of scope or are reassigned
  - Memory leaks in loops and local scopes now prevented
  - **Known Limitation**: Complex ownership transfer across function boundaries needs refinement
  - **Tested**: Basic memory management scenarios work correctly

## Previous Changes (September 14, 2025)
- **GitHub Import Setup**: Successfully imported from GitHub and configured for Replit environment
- **Workflow Configuration**: Configured "Orion Web IDE" workflow with webview output type and port 5000 binding for proper user interface visibility
- **Deployment Configuration**: Set up autoscale deployment with gunicorn for production ready deployment
- **Environment Compatibility**: Verified UV package manager working with Python dependencies from pyproject.toml
- **Compiler Testing**: Tested C++ Orion compiler binary - web API endpoints working perfectly with proper JSON responses
- **Web Interface**: Confirmed frontend working with Bootstrap 5, Prism.js syntax highlighting, and Feather icons
- **API Endpoints**: All endpoints functional - /compile, /check-syntax, /ast, and static file serving
- **Parameter Passing Bug Fix**: **CRITICAL BUG FIXED** - Resolved parameter passing mechanism where function parameters were returning garbage values instead of correct arguments
  - **Root Cause**: Functions were generated inline within main execution stream, causing control flow to fall through into function bodies immediately
  - **Solution**: Split assembly generation into separate buffers (funcsAsm for functions, assembly for main execution)  
  - **Implementation**: Fixed function call mechanism to use proper System V AMD64 calling convention with real `call` instructions
  - **Testing**: Verified all parameter scenarios now work correctly (e.g., `add(5,6)` returns `11`, `test(42)` returns `42`, `multiply(2,3,4)` returns `24`)
- **Import Complete**: Project fully operational with all critical bugs resolved and ready for production use

## System Architecture
### Frontend Architecture
The frontend is a single-page application built with vanilla JavaScript and Bootstrap 5 for responsive design. It includes a custom code editor with Prism.js for syntax highlighting and Feather icons for UI elements. Real-time compilation requests are handled via JavaScript.

### Backend Architecture
The backend uses Flask with CORS support to serve the web interface and interact with the native C++ compiler. It provides RESTful endpoints for compilation and execution. The execution model involves direct machine code generation without interpretation.

### Compiler Architecture
The Orion compiler is written in C++ and follows a traditional pipeline:
- **Lexer**: Tokenizes source code.
- **Parser**: Builds an Abstract Syntax Tree (AST) using recursive descent.
- **Type Checker**: Validates types and semantics via a visitor pattern.
- **Code Generator**: Produces x86-64 assembly code from the AST.
- **Binary**: A standalone executable for compiling and running Orion programs.

### Language Design Philosophy
Orion is a pure compiled language designed for performance and readability:
- **Type System**: Explicit type annotations (int, float, string, bool).
- **Function Syntax**: Clear declarations with return type annotations.
- **Control Flow**: C-style structures (if/else, for, while) with Python-like readability.
- **Memory Management**: Manual memory management with safer syntax.
- **Compilation Model**: Direct to machine code.
- **Function Behavior**: Python-style function execution, where functions only run when called, supporting nested functions and explicit calls (including `main()`).
- **Variable Scoping**: Python-style scoping with function-scope isolation, local variables by default, and `global` keyword support.
- **`const` Keyword**: Supports immutable variables with compile-time validation and clear error messages.
- **Arithmetic Operators**: Full Python-style arithmetic operators (+, -, \*, /, %, //, \*\*) with proper precedence and x86-64 assembly generation.
- **Compound Assignment Operators**: Python-style compound assignment operators (+=, -=, \*=, /=, %=) implemented via desugaring at parse time.
- **Loop Implementation**: Comprehensive Python-style loop support including `while` and `for-in` loops, with `break`, `continue`, and `pass` statements, and proper handling of nested loops.

## External Dependencies
### Frontend Dependencies
- **Bootstrap 5.3.0**: CSS framework.
- **Prism.js 1.29.0**: Syntax highlighting library.
- **Feather Icons**: Lightweight icon library.

### Backend Dependencies
- **Flask**: Python web framework for serving the web interface.
- **Flask-CORS**: Cross-Origin Resource Sharing extension for API endpoints.
- **C++ Compiler**: Native g++ for building the Orion compiler (pre-compiled binary included).
- **Gunicorn**: Production WSGI HTTP Server for deployment.

## Environment Setup
### Development Environment
- **Python 3.11+** with UV package manager
- **Workflow**: "Orion Web IDE" runs on port 5000 with webview output
- **Host Configuration**: Bound to 0.0.0.0 for Replit proxy compatibility

### Deployment Configuration  
- **Target**: Autoscale deployment for stateless web application
- **Command**: `gunicorn --bind 0.0.0.0:5000 --reuse-port main:app`
- **Port**: 5000 (frontend web interface)

### Key Files
- `app.py`: Main Flask application with compilation endpoints
- `main.py`: Application entry point for deployment
- `index.html`: Web IDE frontend interface  
- `compiler/orion`: Pre-built Orion language compiler executable
- `examples/`: Sample Orion programs (hello.or, fibonacci.or)