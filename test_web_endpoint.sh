#!/bin/bash
echo "Testing Orion web compilation endpoint..."
curl -X POST http://localhost:5000/compile \
  -H "Content-Type: application/json" \
  -d "{\"code\": \"fn main() { out(\\\"Hello from web interface!\\\"); }\"}" \
  -s
