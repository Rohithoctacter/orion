#!/usr/bin/env python3
"""
Main application entry point for Orion Compiler Web Interface.
This file is used by gunicorn to serve the application.
"""

from app import app

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)