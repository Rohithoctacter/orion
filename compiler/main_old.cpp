#include "ast.h"
#include "lexer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

// Forward declarations
namespace orion {
class Parser;
class CodeGenerator;
}

class SimpleOrionParser {
private:
    std::string source;
    std::unordered_map<std::string, std::string> variables;
    
public:
    SimpleOrionParser(const std::string& src) : source(src) {}
    
    struct ParsedStatement {
        enum Type { ASSIGNMENT, OUTPUT, DTYPE_CALL, TUPLE_ASSIGNMENT, CHAINED_ASSIGNMENT };
        Type type;
        std::string variable;
        std::string value;
        std::vector<std::string> args;
        // For tuple assignment
        std::vector<std::string> leftVars;
        std::vector<std::string> rightVars;
        // For chained assignment
        std::vector<std::string> chainedVars;
    };
    
    std::vector<ParsedStatement> parse() {
        std::vector<ParsedStatement> statements;
        
        // Handle both simple format and fn main() {} format
        std::string processedSource = preprocessOrionSyntax(source);
        
        std::string line;
        std::istringstream stream(processedSource);
        
        while (std::getline(stream, line)) {
            // Remove leading/trailing whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            if (line.empty()) continue;
            
            // Parse assignments
            size_t assignPos = line.find('=');
            if (assignPos != std::string::npos) {
                // Check for chained assignment (a=b=5)
                std::vector<size_t> assignPositions;
                for (size_t i = 0; i < line.length(); i++) {
                    if (line[i] == '=') {
                        assignPositions.push_back(i);
                    }
                }
                
                if (assignPositions.size() > 1) {
                    // Chained assignment detected
                    ParsedStatement stmt;
                    stmt.type = ParsedStatement::CHAINED_ASSIGNMENT;
                    
                    // Extract variables (everything before the last =)
                    size_t lastAssignPos = assignPositions.back();
                    std::string varsString = line.substr(0, lastAssignPos);
                    
                    // Extract value (everything after the last =)
                    std::string value = line.substr(lastAssignPos + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    if (!value.empty() && value.back() == ';') {
                        value.pop_back();
                        value.erase(value.find_last_not_of(" \t") + 1);
                    }
                    stmt.value = value;
                    
                    // Parse chained variables (split by =)
                    std::string var;
                    for (size_t i = 0; i < varsString.length(); i++) {
                        char c = varsString[i];
                        if (c == '=') {
                            var.erase(0, var.find_first_not_of(" \t"));
                            var.erase(var.find_last_not_of(" \t") + 1);
                            if (!var.empty()) {
                                stmt.chainedVars.push_back(var);
                            }
                            var.clear();
                        } else {
                            var += c;
                        }
                    }
                    // Add final variable
                    var.erase(0, var.find_first_not_of(" \t"));
                    var.erase(var.find_last_not_of(" \t") + 1);
                    if (!var.empty()) {
                        stmt.chainedVars.push_back(var);
                    }
                    
                    statements.push_back(stmt);
                    continue;
                }
                
                std::string leftSide = line.substr(0, assignPos);
                leftSide.erase(leftSide.find_last_not_of(" \t") + 1);
                
                std::string rightSide = line.substr(assignPos + 1);
                rightSide.erase(0, rightSide.find_first_not_of(" \t"));
                if (!rightSide.empty() && rightSide.back() == ';') {
                    rightSide.pop_back();
                    rightSide.erase(rightSide.find_last_not_of(" \t") + 1);
                }
                
                // Check for tuple assignment: (a,b) = (b,a)
                if (leftSide.front() == '(' && leftSide.back() == ')' && 
                    rightSide.front() == '(' && rightSide.back() == ')') {
                    ParsedStatement stmt;
                    stmt.type = ParsedStatement::TUPLE_ASSIGNMENT;
                    
                    // Parse left side variables
                    std::string leftVars = leftSide.substr(1, leftSide.length() - 2);
                    parseTupleVariables(leftVars, stmt.leftVars);
                    
                    // Parse right side variables
                    std::string rightVars = rightSide.substr(1, rightSide.length() - 2);
                    parseTupleVariables(rightVars, stmt.rightVars);
                    
                    statements.push_back(stmt);
                    continue;
                } else {
                    // Regular variable assignment (a = 5)
                    ParsedStatement stmt;
                    stmt.type = ParsedStatement::ASSIGNMENT;
                    
                    stmt.variable = leftSide;
                    stmt.value = rightSide;
                    statements.push_back(stmt);
                    continue;
                }
            }
            
            // Parse out() calls
            if (line.find("out(") != std::string::npos) {
                ParsedStatement stmt;
                stmt.type = ParsedStatement::OUTPUT;
                
                size_t start = line.find('(') + 1;
                size_t end = line.find(')', start);
                std::string args = line.substr(start, end - start);
                
                // Handle dtype() calls inside out()
                if (args.find("dtype(") != std::string::npos) {
                    stmt.type = ParsedStatement::DTYPE_CALL;
                    size_t dtypeStart = args.find("dtype(") + 6;
                    size_t dtypeEnd = args.find(')', dtypeStart);
                    std::string dtypeArg = args.substr(dtypeStart, dtypeEnd - dtypeStart);
                    dtypeArg.erase(0, dtypeArg.find_first_not_of(" \t"));
                    dtypeArg.erase(dtypeArg.find_last_not_of(" \t") + 1);
                    stmt.args.push_back(dtypeArg);
                } else {
                    // Split arguments by comma, but respect quotes
                    std::string arg;
                    bool inQuotes = false;
                    char quoteChar = '\0';
                    
                    for (size_t i = 0; i < args.length(); i++) {
                        char c = args[i];
                        
                        if (!inQuotes && (c == '"' || c == '\'')) {
                            // Start of quoted string
                            inQuotes = true;
                            quoteChar = c;
                            arg += c;
                        } else if (inQuotes && c == quoteChar) {
                            // End of quoted string
                            inQuotes = false;
                            arg += c;
                        } else if (!inQuotes && c == ',') {
                            // Comma outside quotes - end current argument
                            arg.erase(0, arg.find_first_not_of(" \t"));
                            arg.erase(arg.find_last_not_of(" \t") + 1);
                            if (!arg.empty()) {
                                stmt.args.push_back(arg);
                            }
                            arg.clear();
                        } else {
                            // Regular character
                            arg += c;
                        }
                    }
                    
                    // Add final argument
                    arg.erase(0, arg.find_first_not_of(" \t"));
                    arg.erase(arg.find_last_not_of(" \t") + 1);
                    if (!arg.empty()) {
                        stmt.args.push_back(arg);
                    }
                }
                
                statements.push_back(stmt);
                continue;
            }
            
            // Parse dtype() calls
            if (line.find("dtype(") != std::string::npos) {
                ParsedStatement stmt;
                stmt.type = ParsedStatement::DTYPE_CALL;
                
                size_t start = line.find('(') + 1;
                size_t end = line.find(')', start);
                std::string arg = line.substr(start, end - start);
                
                arg.erase(0, arg.find_first_not_of(" \t"));
                arg.erase(arg.find_last_not_of(" \t") + 1);
                stmt.args.push_back(arg);
                
                statements.push_back(stmt);
                continue;
            }
        }
        
        return statements;
    }
    
private:
    void parseTupleVariables(const std::string& varStr, std::vector<std::string>& vars) {
        std::string var;
        for (size_t i = 0; i < varStr.length(); i++) {
            char c = varStr[i];
            if (c == ',') {
                // End current variable
                var.erase(0, var.find_first_not_of(" \t"));
                var.erase(var.find_last_not_of(" \t") + 1);
                if (!var.empty()) {
                    vars.push_back(var);
                }
                var.clear();
            } else {
                var += c;
            }
        }
        // Add final variable
        var.erase(0, var.find_first_not_of(" \t"));
        var.erase(var.find_last_not_of(" \t") + 1);
        if (!var.empty()) {
            vars.push_back(var);
        }
    }
    std::string preprocessOrionSyntax(const std::string& source) {
        std::string processed = source;
        
        // Find fn main() { } blocks and extract their contents
        std::regex fnMainRegex(R"(fn\s+main\s*\(\s*\)\s*\{([^}]*)\})");
        std::smatch match;
        
        if (std::regex_search(processed, match, fnMainRegex)) {
            // Extract the content inside the main function
            std::string mainBody = match[1].str();
            
            // Clean up the extracted content
            std::istringstream bodyStream(mainBody);
            std::string cleanedBody;
            std::string line;
            
            while (std::getline(bodyStream, line)) {
                // Remove leading/trailing whitespace
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t") + 1);
                
                if (!line.empty()) {
                    cleanedBody += line + "\n";
                }
            }
            
            processed = cleanedBody;
        }
        
        return processed;
    }
};

class OrionAssemblyGenerator {
private:
    std::ostringstream assembly;
    int labelCounter = 0;
    std::unordered_map<std::string, std::string> variables;
    
    std::string newLabel() {
        return "L" + std::to_string(labelCounter++);
    }
    
    std::string getDataType(const std::string& value) {
        if (value == "True" || value == "False" || value == "true" || value == "false") return "bool";
        if (!value.empty() && value.front() == '"' && value.back() == '"') return "string";
        if (std::all_of(value.begin(), value.end(), ::isdigit) || 
            (value[0] == '-' && std::all_of(value.begin() + 1, value.end(), ::isdigit))) return "int";
        if (value.find('.') != std::string::npos) return "float";
        
        auto it = variables.find(value);
        if (it != variables.end()) return getDataType(it->second);
        return "unknown";
    }
    
public:
    std::string generate(const std::vector<SimpleOrionParser::ParsedStatement>& statements) {
        assembly.str("");
        assembly.clear();
        
        // First pass: collect all strings and floats
        std::vector<std::string> stringLiterals;
        
        for (const auto& stmt : statements) {
            if (stmt.type == SimpleOrionParser::ParsedStatement::ASSIGNMENT) {
                if (!stmt.value.empty() && stmt.value.front() == '"' && stmt.value.back() == '"') {
                    std::string content = stmt.value.substr(1, stmt.value.length() - 2);
                    stringLiterals.push_back(content);
                } else if (stmt.value.find('.') != std::string::npos) {
                    stringLiterals.push_back(stmt.value);  // Store float as string
                }
            }
            if (stmt.type == SimpleOrionParser::ParsedStatement::OUTPUT) {
                for (const auto& arg : stmt.args) {
                    if (!arg.empty() && arg.front() == '"' && arg.back() == '"') {
                        std::string content = arg.substr(1, arg.length() - 2);
                        stringLiterals.push_back(content);
                    } else if (arg.find('.') != std::string::npos) {
                        stringLiterals.push_back(arg);  // Store float as string
                    }
                    // Also check for string variables that will need data
                    auto it = variables.find(arg);
                    if (it != variables.end()) {
                        std::string varValue = it->second;
                        if (!varValue.empty() && varValue.front() == '"' && varValue.back() == '"') {
                            std::string content = varValue.substr(1, varValue.length() - 2);
                            stringLiterals.push_back(content);
                        } else if (varValue.find('.') != std::string::npos) {
                            stringLiterals.push_back(varValue);
                        }
                    }
                }
            }
        }
        
        // Assembly header
        assembly << ".section .data\n";
        assembly << "format_str: .string \"%s\\n\"\n";
        assembly << "format_int: .string \"%d\\n\"\n";
        assembly << "format_type: .string \"datatype : %s\\n\"\n";
        assembly << "type_int: .string \"int\"\n";
        assembly << "type_string: .string \"string\"\n";
        assembly << "type_bool: .string \"bool\"\n";
        assembly << "type_float: .string \"float\"\n";
        
        // Generate string literals
        for (size_t i = 0; i < stringLiterals.size(); i++) {
            assembly << "str" << i << ": .string \"" << stringLiterals[i] << "\"\n";
        }
        
        assembly << "\n.section .text\n";
        assembly << ".global main\n";
        assembly << "\nmain:\n";
        
        // Generate code for each statement
        int stringCounter = 0;
        for (const auto& stmt : statements) {
            switch (stmt.type) {
                case SimpleOrionParser::ParsedStatement::ASSIGNMENT:
                    generateAssignment(stmt, stringCounter);
                    break;
                case SimpleOrionParser::ParsedStatement::OUTPUT:
                    generateOutput(stmt, stringCounter);
                    break;
                case SimpleOrionParser::ParsedStatement::DTYPE_CALL:
                    generateDtype(stmt);
                    break;
                case SimpleOrionParser::ParsedStatement::TUPLE_ASSIGNMENT:
                    generateTupleAssignment(stmt);
                    break;
                case SimpleOrionParser::ParsedStatement::CHAINED_ASSIGNMENT:
                    generateChainedAssignment(stmt, stringCounter);
                    break;
            }
        }
        
        // Return from main
        assembly << "\n    # Return from main\n";
        assembly << "    mov $0, %rax\n";   // return 0
        assembly << "    ret\n";
        
        return assembly.str();
    }
    
private:
    void generateAssignment(const SimpleOrionParser::ParsedStatement& stmt, int& stringCounter) {
        assembly << "\n    # Assignment: " << stmt.variable << " = " << stmt.value << "\n";
        
        // Resolve the value - check if it's a variable reference first
        std::string resolvedValue = stmt.value;
        auto it = variables.find(stmt.value);
        if (it != variables.end()) {
            // Variable reference - use its value
            resolvedValue = it->second;
            assembly << "    # Resolving variable reference: " << stmt.value << " -> " << resolvedValue << "\n";
        }
        
        // Store the resolved value
        variables[stmt.variable] = resolvedValue;
        
        if (!resolvedValue.empty() && resolvedValue.front() == '"' && resolvedValue.back() == '"') {
            // String assignment - store reference to string literal
            assembly << "    mov $str" << stringCounter << ", %rax\n";
            assembly << "    # Store string reference for " << stmt.variable << "\n";
            stringCounter++;
        } else if (std::all_of(resolvedValue.begin(), resolvedValue.end(), ::isdigit) || 
                   (resolvedValue[0] == '-' && std::all_of(resolvedValue.begin() + 1, resolvedValue.end(), ::isdigit))) {
            // Integer assignment
            assembly << "    mov $" << resolvedValue << ", %rax\n";
            assembly << "    # Store integer " << resolvedValue << " for " << stmt.variable << "\n";
        } else if (resolvedValue.find('.') != std::string::npos) {
            // Float assignment - store as integer representation for now
            assembly << "    mov $0, %rax  # Float " << resolvedValue << " (stored as 0 for now)\n";
            assembly << "    # Store float " << resolvedValue << " for " << stmt.variable << "\n";
        } else if (resolvedValue == "True" || resolvedValue == "true") {
            assembly << "    mov $1, %rax\n";
            assembly << "    # Store boolean True for " << stmt.variable << "\n";
        } else if (resolvedValue == "False" || resolvedValue == "false") {
            assembly << "    mov $0, %rax\n";
            assembly << "    # Store boolean False for " << stmt.variable << "\n";
        }
    }
    
    void generateOutput(const SimpleOrionParser::ParsedStatement& stmt, int& stringCounter) {
        assembly << "\n    # Output: out(";
        for (size_t i = 0; i < stmt.args.size(); i++) {
            if (i > 0) assembly << ", ";
            assembly << stmt.args[i];
        }
        assembly << ")\n";
        
        for (const auto& arg : stmt.args) {
            if (!arg.empty() && arg.front() == '"' && arg.back() == '"') {
                // String literal
                assembly << "    mov $str" << stringCounter << ", %rdi\n";
                assembly << "    mov $format_str, %rsi\n";
                assembly << "    mov %rdi, %rsi\n";
                assembly << "    mov $format_str, %rdi\n";
                assembly << "    xor %rax, %rax\n";
                assembly << "    call printf\n";
                stringCounter++;
            } else if (std::all_of(arg.begin(), arg.end(), ::isdigit)) {
                // Integer literal
                assembly << "    mov $" << arg << ", %rsi\n";
                assembly << "    mov $format_int, %rdi\n";
                assembly << "    xor %rax, %rax\n";
                assembly << "    call printf\n";
            } else if (arg == "True") {
                // Boolean True
                assembly << "    mov $1, %rsi\n";
                assembly << "    mov $format_int, %rdi\n";
                assembly << "    xor %rax, %rax\n";
                assembly << "    call printf\n";
            } else if (arg == "False") {
                // Boolean False
                assembly << "    mov $0, %rsi\n";
                assembly << "    mov $format_int, %rdi\n";
                assembly << "    xor %rax, %rax\n";
                assembly << "    call printf\n";
            } else {
                // Variable reference
                auto it = variables.find(arg);
                if (it != variables.end()) {
                    std::string varValue = it->second;
                    if (!varValue.empty() && varValue.front() == '"' && varValue.back() == '"') {
                        // String variable
                        std::string content = varValue.substr(1, varValue.length() - 2);
                        assembly << "    # Output string variable " << arg << ": \"" << content << "\"\n";
                        assembly << "    mov $str" << stringCounter << ", %rsi\n";
                        assembly << "    mov $format_str, %rdi\n";
                        assembly << "    xor %rax, %rax\n";
                        assembly << "    call printf\n";
                        stringCounter++;
                    } else if (varValue == "True" || varValue == "true") {
                        // Boolean True variable
                        assembly << "    # Output boolean variable " << arg << ": True\n";
                        assembly << "    mov $1, %rsi\n";
                        assembly << "    mov $format_int, %rdi\n";
                        assembly << "    xor %rax, %rax\n";
                        assembly << "    call printf\n";
                    } else if (varValue == "False" || varValue == "false") {
                        // Boolean False variable
                        assembly << "    # Output boolean variable " << arg << ": False\n";
                        assembly << "    mov $0, %rsi\n";
                        assembly << "    mov $format_int, %rdi\n";
                        assembly << "    xor %rax, %rax\n";
                        assembly << "    call printf\n";
                    } else if (varValue.find('.') != std::string::npos) {
                        // Float variable - output as 0 for now
                        assembly << "    # Output float variable " << arg << ": " << varValue << " (as 0)\n";
                        assembly << "    mov $0, %rsi\n";
                        assembly << "    mov $format_int, %rdi\n";
                        assembly << "    xor %rax, %rax\n";
                        assembly << "    call printf\n";
                    } else {
                        // Integer variable
                        assembly << "    # Output variable " << arg << ": " << varValue << "\n";
                        assembly << "    mov $" << varValue << ", %rsi\n";
                        assembly << "    mov $format_int, %rdi\n";
                        assembly << "    xor %rax, %rax\n";
                        assembly << "    call printf\n";
                    }
                }
            }
        }
    }
    
    void generateDtype(const SimpleOrionParser::ParsedStatement& stmt) {
        assembly << "\n    # dtype(" << stmt.args[0] << ")\n";
        
        std::string arg = stmt.args[0];
        std::string type = getDataType(arg);
        
        assembly << "    mov $format_type, %rdi\n";
        assembly << "    mov $type_" << type << ", %rsi\n";
        assembly << "    xor %rax, %rax\n";
        assembly << "    call printf\n";
    }
    
    void generateTupleAssignment(const SimpleOrionParser::ParsedStatement& stmt) {
        assembly << "\n    # Tuple assignment: (";
        for (size_t i = 0; i < stmt.leftVars.size(); i++) {
            if (i > 0) assembly << ", ";
            assembly << stmt.leftVars[i];
        }
        assembly << ") = (";
        for (size_t i = 0; i < stmt.rightVars.size(); i++) {
            if (i > 0) assembly << ", ";
            assembly << stmt.rightVars[i];
        }
        assembly << ")\n";
        
        // Perform the variable swapping
        if (stmt.leftVars.size() == stmt.rightVars.size()) {
            // Store old values temporarily
            std::vector<std::string> tempValues;
            for (size_t i = 0; i < stmt.rightVars.size(); i++) {
                auto it = variables.find(stmt.rightVars[i]);
                if (it != variables.end()) {
                    tempValues.push_back(it->second);
                    assembly << "    # Temp store " << stmt.rightVars[i] << " = " << it->second << "\n";
                } else {
                    tempValues.push_back(stmt.rightVars[i]); // Direct value if not a variable
                    assembly << "    # Temp store literal " << stmt.rightVars[i] << "\n";
                }
            }
            
            // Assign new values
            for (size_t i = 0; i < stmt.leftVars.size(); i++) {
                variables[stmt.leftVars[i]] = tempValues[i];
                assembly << "    # Set " << stmt.leftVars[i] << " = " << tempValues[i] << "\n";
            }
        }
    }
    
    void generateChainedAssignment(const SimpleOrionParser::ParsedStatement& stmt, int& stringCounter) {
        assembly << "\n    # Chained assignment: ";
        for (size_t i = 0; i < stmt.chainedVars.size(); i++) {
            if (i > 0) assembly << "=";
            assembly << stmt.chainedVars[i];
        }
        assembly << "=" << stmt.value << "\n";
        
        // Resolve the value - check if it's a variable reference first
        std::string resolvedValue = stmt.value;
        auto it = variables.find(stmt.value);
        if (it != variables.end()) {
            // Variable reference - use its value
            resolvedValue = it->second;
            assembly << "    # Resolving variable reference: " << stmt.value << " -> " << resolvedValue << "\n";
        }
        
        // Assign the resolved value to all chained variables
        for (const auto& var : stmt.chainedVars) {
            variables[var] = resolvedValue;
            assembly << "    # Set " << var << " = " << resolvedValue << "\n";
            
            if (!resolvedValue.empty() && resolvedValue.front() == '"' && resolvedValue.back() == '"') {
                // String assignment - store reference to string literal
                assembly << "    mov $str" << stringCounter << ", %rax\n";
                assembly << "    # Store string reference for " << var << "\n";
            } else if (std::all_of(resolvedValue.begin(), resolvedValue.end(), ::isdigit) || 
                       (resolvedValue[0] == '-' && std::all_of(resolvedValue.begin() + 1, resolvedValue.end(), ::isdigit))) {
                // Integer assignment
                assembly << "    mov $" << resolvedValue << ", %rax\n";
                assembly << "    # Store integer " << resolvedValue << " for " << var << "\n";
            } else if (resolvedValue.find('.') != std::string::npos) {
                // Float assignment - store as integer representation for now
                assembly << "    mov $0, %rax  # Float " << resolvedValue << " (stored as 0 for now)\n";
                assembly << "    # Store float " << resolvedValue << " for " << var << "\n";
            } else if (resolvedValue == "True" || resolvedValue == "true") {
                assembly << "    mov $1, %rax\n";
                assembly << "    # Store boolean True for " << var << "\n";
            } else if (resolvedValue == "False" || resolvedValue == "false") {
                assembly << "    mov $0, %rax\n";
                assembly << "    # Store boolean False for " << var << "\n";
            }
        }
        
        // Only increment string counter once if it was a string
        if (!resolvedValue.empty() && resolvedValue.front() == '"' && resolvedValue.back() == '"') {
            stringCounter++;
        }
    }
};

}

// True compilation function
extern "C" {
    struct CompilationResult {
        bool success;
        char* output;
        char* error;
        int execution_time;
    };
    
    CompilationResult* compile_orion(const char* source_code) {
        CompilationResult* result = new CompilationResult();
        
        // Start timing
        auto start_time = std::chrono::high_resolution_clock::now();
        
        try {
            std::string code(source_code);
            
            // Parse the Orion source code
            orion::SimpleOrionParser parser(code);
            auto statements = parser.parse();
            
            // Generate assembly code
            orion::OrionAssemblyGenerator generator;
            std::string assembly = generator.generate(statements);
            
            // Write assembly to temporary file
            std::string tempAsmFile = "/tmp/orion_prog.s";
            std::string tempExeFile = "/tmp/orion_prog";
            
            std::ofstream asmFile(tempAsmFile);
            if (!asmFile) {
                result->success = false;
                std::string errorMsg = "Error: Could not create assembly file\n";
                result->error = new char[errorMsg.length() + 1];
                strcpy(result->error, errorMsg.c_str());
                result->output = new char[1];
                result->output[0] = '\0';
                result->execution_time = 0;
                return result;
            }
            
            asmFile << assembly;
            asmFile.close();
            
            // Compile assembly to executable using GCC
            std::string compileCmd = "gcc -o " + tempExeFile + " " + tempAsmFile + " 2>&1";
            FILE* pipe = popen(compileCmd.c_str(), "r");
            if (!pipe) {
                result->success = false;
                std::string errorMsg = "Error: Could not run assembler\n";
                result->error = new char[errorMsg.length() + 1];
                strcpy(result->error, errorMsg.c_str());
                result->output = new char[1];
                result->output[0] = '\0';
                result->execution_time = 0;
                return result;
            }
            
            std::string compileOutput;
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                compileOutput += buffer;
            }
            int compileResult = pclose(pipe);
            
            if (compileResult != 0) {
                result->success = false;
                std::string errorMsg = "Assembly compilation failed:\n" + compileOutput;
                result->error = new char[errorMsg.length() + 1];
                strcpy(result->error, errorMsg.c_str());
                result->output = new char[1];
                result->output[0] = '\0';
                result->execution_time = 0;
                remove(tempAsmFile.c_str());
                return result;
            }
            
            // Execute the compiled binary
            FILE* execPipe = popen(tempExeFile.c_str(), "r");
            if (!execPipe) {
                result->success = false;
                std::string errorMsg = "Error: Could not execute compiled program\n";
                result->error = new char[errorMsg.length() + 1];
                strcpy(result->error, errorMsg.c_str());
                result->output = new char[1];
                result->output[0] = '\0';
                result->execution_time = 0;
                remove(tempAsmFile.c_str());
                remove(tempExeFile.c_str());
                return result;
            }
            
            std::string programOutput;
            while (fgets(buffer, sizeof(buffer), execPipe) != NULL) {
                programOutput += buffer;
            }
            pclose(execPipe);
            
            // Keep assembly file for demonstration, clean up executable
            // remove(tempAsmFile.c_str());  // Keep assembly file to prove compilation
            remove(tempExeFile.c_str());
            
            // Success
            result->success = true;
            result->error = new char[1];
            result->error[0] = '\0';
            
            if (programOutput.empty()) {
                programOutput = "Program compiled and executed successfully";
            }
            
            result->output = new char[programOutput.length() + 1];
            strcpy(result->output, programOutput.c_str());
            
            // Calculate actual execution time
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            result->execution_time = duration.count();
            
        } catch (const std::exception& e) {
            result->success = false;
            std::string error = std::string("Compilation error: ") + e.what() + "\n";
            result->error = new char[error.length() + 1];
            strcpy(result->error, error.c_str());
            result->output = new char[1];
            result->output[0] = '\0';
            result->execution_time = 0;
        }
        
        return result;
    }
    
    void free_result(CompilationResult* result) {
        if (result) {
            delete[] result->output;
            delete[] result->error;
            delete result;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <orion_file>\n";
        return 1;
    }
    
    std::string filename = argv[1];
    std::ifstream file(filename);
    
    if (!file) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        return 1;
    }
    
    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    CompilationResult* result = compile_orion(source.c_str());
    
    if (result->success) {
        std::cout << result->output;
        free_result(result);
        return 0;
    } else {
        std::cerr << result->error;
        free_result(result);
        return 1;
    }
}