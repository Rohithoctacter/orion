#include "ast.h"
#include "lexer.h"
#include "simple_parser.h"
#include "types.cpp"
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
#include <unordered_set>
#include <stack>

namespace orion {


// Simplified Code Generator for basic functionality
class SimpleCodeGenerator : public ASTVisitor {
private:
    std::ostringstream assembly;      // For main execution code
    std::ostringstream funcsAsm;      // For function definitions
    std::vector<std::string> stringLiterals;
    std::vector<double> floatLiterals;
    struct VariableInfo {
        int stackOffset;
        std::string type;
        bool isGlobal;
        bool isConstant;
    };
    std::unordered_map<std::string, VariableInfo> globalVariables; // Global scope variables
    std::unordered_map<std::string, VariableInfo> localVariables; // Current function scope variables
    std::unordered_set<std::string> declaredGlobal; // Variables explicitly declared global with 'global' keyword
    std::unordered_set<std::string> declaredLocal;  // Variables explicitly declared local with 'local' keyword
    std::unordered_set<std::string> constantVariables; // Variables declared as const
    
    // Hierarchical function storage for proper scoping
    struct FunctionScope {
        std::unordered_map<std::string, FunctionDeclaration*> functions;
        std::string parentFunction; // Name of parent function (empty for global scope)
    };
    
    std::unordered_map<std::string, FunctionScope> functionScopes; // Scope name -> functions in that scope
    std::vector<std::string> functionCallStack; // Track current function execution stack
    
    int stackOffset = 0;
    bool inFunction = false;
    int labelCounter = 0;
    
    // For managing nested loops and break/continue statements
    std::stack<std::string> breakLabels;
    std::stack<std::string> continueLabels;
    
    std::string newLabel(const std::string& prefix = "L") {
        return prefix + std::to_string(labelCounter++);
    }
    
    void setVariable(const std::string& varName, const std::string& valueRegister) {
        setVariable(varName, valueRegister, "unknown");
    }
    
    void setVariable(const std::string& varName, const std::string& valueRegister, const std::string& varType) {
        // Look up existing variable
        auto varInfo = lookupVariable(varName);
        
        if (varInfo == nullptr) {
            // Create new variable with proper scoping
            stackOffset += 8;
            VariableInfo newVarInfo;
            newVarInfo.stackOffset = stackOffset;
            newVarInfo.type = varType;
            newVarInfo.isConstant = false;
            
            if (inFunction && !declaredGlobal.count(varName)) {
                // Create local variable
                newVarInfo.isGlobal = false;
                localVariables[varName] = newVarInfo;
                varInfo = &localVariables[varName];
            } else {
                // Create global variable
                newVarInfo.isGlobal = true;
                globalVariables[varName] = newVarInfo;
                varInfo = &globalVariables[varName];
            }
        } else {
            // Update existing variable's type if specified
            if (varType != "unknown") {
                varInfo->type = varType;
            }
        }
        
        // Store value from register to variable's stack slot
        assembly << "    mov " << valueRegister << ", -" << varInfo->stackOffset << "(%rbp)  # " << varName << " = " << valueRegister << " (type: " << varInfo->type << ")\n";
    }
    
    bool isFloatExpression(Expression* expr) {
        if (auto floatLit = dynamic_cast<FloatLiteral*>(expr)) {
            return true;
        }
        if (auto id = dynamic_cast<Identifier*>(expr)) {
            auto varInfo = lookupVariable(id->name);
            return varInfo && varInfo->type == "float";
        }
        return false;
    }
    
    // Expression kind inference for type safety
    enum class ExprKind { INT, FLOAT, BOOL, STRING, LIST, UNKNOWN };
    
    ExprKind inferExprKind(Expression* expr) {
        if (auto intLit = dynamic_cast<IntLiteral*>(expr)) {
            return ExprKind::INT;
        }
        if (auto floatLit = dynamic_cast<FloatLiteral*>(expr)) {
            return ExprKind::FLOAT;
        }
        if (auto boolLit = dynamic_cast<BoolLiteral*>(expr)) {
            return ExprKind::BOOL;
        }
        if (auto strLit = dynamic_cast<StringLiteral*>(expr)) {
            return ExprKind::STRING;
        }
        if (auto listLit = dynamic_cast<ListLiteral*>(expr)) {
            return ExprKind::LIST;
        }
        if (auto id = dynamic_cast<Identifier*>(expr)) {
            auto var = lookupVariable(id->name);
            if (var) {
                if (var->type == "int") return ExprKind::INT;
                if (var->type == "float") return ExprKind::FLOAT;
                if (var->type == "bool") return ExprKind::BOOL;
                if (var->type == "string") return ExprKind::STRING;
                if (var->type == "list") return ExprKind::LIST;
            }
        }
        if (auto binExpr = dynamic_cast<BinaryExpression*>(expr)) {
            ExprKind leftKind = inferExprKind(binExpr->left.get());
            ExprKind rightKind = inferExprKind(binExpr->right.get());
            
            if (binExpr->op == BinaryOp::ADD) {
                if (leftKind == ExprKind::LIST && rightKind == ExprKind::LIST) {
                    return ExprKind::LIST;  // List concatenation
                }
            }
            if (binExpr->op == BinaryOp::MUL) {
                if ((leftKind == ExprKind::LIST && rightKind == ExprKind::INT) ||
                    (leftKind == ExprKind::INT && rightKind == ExprKind::LIST)) {
                    return ExprKind::LIST;  // List repetition
                }
            }
            
            // Default to numeric operations
            if (leftKind == ExprKind::FLOAT || rightKind == ExprKind::FLOAT) {
                return ExprKind::FLOAT;
            }
            return ExprKind::INT;
        }
        return ExprKind::UNKNOWN;
    }
    
    int addStringLiteral(const std::string& str) {
        stringLiterals.push_back(str);
        return stringLiterals.size() - 1;
    }
    
    int addFloatLiteral(double value) {
        floatLiterals.push_back(value);
        return floatLiterals.size() - 1;
    }
    
public:
    std::string generate(Program& program) {
        assembly.str("");
        assembly.clear();
        stringLiterals.clear();
        floatLiterals.clear();
        globalVariables.clear();
        localVariables.clear();
        declaredGlobal.clear();
        declaredLocal.clear();
        constantVariables.clear();
        inFunction = false;
        stackOffset = 0;
        labelCounter = 0;
        
        // Visit program to collect strings and generate code
        program.accept(*this);
        
        // Generate complete assembly
        std::ostringstream fullAssembly;
        
        // Data section
        fullAssembly << ".section .data\n";
        fullAssembly << "format_int: .string \"%d\\n\"\n";
        fullAssembly << "format_str: .string \"%s\"\n";
        fullAssembly << "format_float: .string \"%.2f\\n\"\n";
        fullAssembly << "dtype_int: .string \"datatype: int\\n\"\n";
        fullAssembly << "dtype_string: .string \"datatype: string\\n\"\n";
        fullAssembly << "dtype_bool: .string \"datatype: bool\\n\"\n";
        fullAssembly << "dtype_float: .string \"datatype: float\\n\"\n";
        fullAssembly << "dtype_list: .string \"datatype: list\\n\"\n";
        fullAssembly << "dtype_unknown: .string \"datatype: unknown\\n\"\n";
        fullAssembly << "str_true: .string \"True\\n\"\n";
        fullAssembly << "str_false: .string \"False\\n\"\n";
        fullAssembly << "str_index_error: .string \"Index Error\\n\"\n";
        
        // String literals
        for (size_t i = 0; i < stringLiterals.size(); i++) {
            fullAssembly << "str_" << i << ": .string \"" << stringLiterals[i] << "\"\n";
        }
        
        // Add float literals  
        for (size_t i = 0; i < floatLiterals.size(); ++i) {
            fullAssembly << "float_" << i << ": .quad " << *reinterpret_cast<uint64_t*>(&floatLiterals[i]) << "\n";
        }
        
        // Text section
        fullAssembly << "\n.section .text\n";
        fullAssembly << ".global main\n";
        fullAssembly << ".extern printf\n";
        fullAssembly << ".extern orion_malloc\n";
        fullAssembly << ".extern orion_free\n";
        fullAssembly << ".extern exit\n";
        fullAssembly << ".extern fmod\n";
        fullAssembly << ".extern pow\n";
        fullAssembly << ".extern strcmp\n";
        // Enhanced list runtime functions
        fullAssembly << ".extern list_new\n";
        fullAssembly << ".extern list_from_data\n";
        fullAssembly << ".extern list_len\n";
        fullAssembly << ".extern list_get\n";
        fullAssembly << ".extern list_set\n";
        fullAssembly << ".extern list_append\n";
        fullAssembly << ".extern list_pop\n";
        fullAssembly << ".extern list_insert\n";
        fullAssembly << ".extern list_concat\n";
        fullAssembly << ".extern list_repeat\n";
        fullAssembly << ".extern list_extend\n";
        fullAssembly << ".extern orion_input\n";
        fullAssembly << ".extern orion_input_prompt\n";
        // String conversion and concatenation functions
        fullAssembly << ".extern int_to_string\n";
        fullAssembly << ".extern float_to_string\n";
        fullAssembly << ".extern bool_to_string\n";
        fullAssembly << ".extern string_to_string\n";
        fullAssembly << ".extern string_concat_parts\n\n";
        
        // Emit user-defined functions first
        fullAssembly << funcsAsm.str();
        
        // Main function (C runtime entry point)
        fullAssembly << "main:\n";
        fullAssembly << "    push %rbp\n";
        fullAssembly << "    mov %rsp, %rbp\n";
        fullAssembly << "    sub $64, %rsp\n";  // Allocate 64 bytes of stack space for variables
        
        // Program code (top-level statements and calls)
        fullAssembly << assembly.str();
        
        // Note: User main function should be called explicitly by user code
        // Don't auto-call main function to allow main() to be used like any other function
        
        // Return 0
        fullAssembly << "    mov $0, %rax\n";
        fullAssembly << "    add $64, %rsp\n";  // Restore stack pointer
        fullAssembly << "    pop %rbp\n";
        fullAssembly << "    ret\n";
        
        return fullAssembly.str();
    }
    
    void visit(Program& node) override {
        // First pass: collect all function definitions with proper scoping
        collectFunctions(node.statements, ""); // Start with global scope
        
        // Second pass: generate assembly code for all collected functions
        generateFunctionAssembly();
        
        // Third pass: execute only non-function statements and function calls
        for (auto& stmt : node.statements) {
            if (dynamic_cast<FunctionDeclaration*>(stmt.get()) == nullptr) {
                stmt->accept(*this);
            }
        }
        
        // Main function will be called from C main in generate() method
    }
    
    void collectFunctions(const std::vector<std::unique_ptr<Statement>>& statements, const std::string& currentScope = "") {
        for (auto& stmt : statements) {
            if (auto func = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                // Store function definition in the appropriate scope
                if (functionScopes.find(currentScope) == functionScopes.end()) {
                    functionScopes[currentScope] = FunctionScope{};
                }
                
                functionScopes[currentScope].functions[func->name] = func;
                assembly << "    # Function '" << func->name << "' defined in scope '" << currentScope << "'\n";
                
                // Recursively collect nested functions from this function's body with proper scope
                if (!func->isSingleExpression) {
                    std::string nestedScope = currentScope.empty() ? func->name : currentScope + "::" + func->name;
                    collectFunctions(func->body, nestedScope);
                }
            } else if (auto block = dynamic_cast<BlockStatement*>(stmt.get())) {
                // Recursively collect nested functions in same scope
                collectFunctions(block->statements, currentScope);
            }
        }
    }
    
    void generateFunctionAssembly() {
        // Generate assembly code for all collected functions in separate buffer
        for (const auto& scope : functionScopes) {
            for (const auto& funcPair : scope.second.functions) {
                const std::string& funcName = funcPair.first;
                FunctionDeclaration* func = funcPair.second;
                
                // Use fn_ prefix to avoid collision with C main
                std::string labelName = (funcName == "main") ? "fn_main" : funcName;
                
                funcsAsm << "\n" << labelName << ":\n";
                funcsAsm << "    push %rbp\n";
                funcsAsm << "    mov %rsp, %rbp\n";
                funcsAsm << "    sub $64, %rsp  # Allocate stack space for local variables\n";
                
                // Save current state and enter function scope
                bool wasInFunction = inFunction;
                auto savedLocalVars = localVariables;
                int savedStackOffset = stackOffset;
                std::ostringstream* savedOutput = nullptr;
                
                // Temporarily redirect output to funcsAsm for function body
                savedOutput = &assembly;
                
                inFunction = true;
                localVariables.clear();
                stackOffset = 0;
                
                // Set up parameters - move from calling convention registers to stack
                const std::string callingConventionRegs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
                
                funcsAsm << "    # Setting up function parameters for " << funcName << "\n";
                for (size_t i = 0; i < func->parameters.size() && i < 6; i++) {
                    const auto& param = func->parameters[i];
                    
                    // Allocate stack slot for parameter
                    stackOffset += 8;
                    VariableInfo paramInfo;
                    paramInfo.stackOffset = stackOffset;
                    // Try to infer parameter type from calling context, default to string for flexibility
                    paramInfo.type = (param.type.toString() != "unknown") ? param.type.toString() : "string";
                    paramInfo.isGlobal = false;
                    paramInfo.isConstant = false;
                    
                    // Register parameter in local variables
                    localVariables[param.name] = paramInfo;
                    
                    // Move parameter from register to stack
                    funcsAsm << "    mov " << callingConventionRegs[i] << ", -" << stackOffset 
                             << "(%rbp)  # Parameter " << param.name << " (type: " << paramInfo.type << ")\n";
                }
                
                // Redirect assembly output to funcsAsm for function body generation
                std::string currentAssembly = assembly.str();
                assembly.str("");
                assembly.clear();
                
                // Generate function body
                if (func->isSingleExpression) {
                    func->expression->accept(*this);
                } else {
                    for (auto& stmt : func->body) {
                        stmt->accept(*this);
                    }
                }
                
                // Move generated body code to funcsAsm and restore assembly
                funcsAsm << assembly.str();
                assembly.str("");
                assembly.clear();
                assembly << currentAssembly;
                
                // Function epilogue - user functions should return to caller
                funcsAsm << "    add $64, %rsp  # Restore stack space\n";
                funcsAsm << "    pop %rbp\n";
                funcsAsm << "    ret\n";
                
                // Restore previous state
                inFunction = wasInFunction;
                localVariables = savedLocalVars;
                stackOffset = savedStackOffset;
            }
        }
    }
    
    void visit(FunctionDeclaration& node) override {
        // Functions are only executed when called, not when defined
        assembly << "    # Function '" << node.name << "' defined but not executed\n";
        
        // Function definitions are collected in the collectFunctions phase
        // Assembly generation happens in generateFunctionAssembly phase
        // This ensures proper scoping and calling convention setup
    }
    
    void executeFunctionCall(const std::string& functionName, const std::vector<std::unique_ptr<Expression>>& arguments) {
        assembly << "    # Executing function call: " << functionName << "\n";
        
        // Find the function in the current scope
        FunctionDeclaration* func = findFunction(functionName);
        if (!func) {
            throw std::runtime_error("Error: Undefined function '" + functionName + "' in current scope");
        }
        
        // Save current function state
        bool wasInFunction = inFunction;
        auto savedLocalVars = localVariables;
        auto savedDeclaredGlobal = declaredGlobal;
        auto savedDeclaredLocal = declaredLocal;
        int savedStackOffset = stackOffset;
        
        // Push function onto call stack for proper scoping
        std::string currentScope = "";
        if (!functionCallStack.empty()) {
            currentScope = functionCallStack.back();
        }
        std::string newScope = currentScope.empty() ? functionName : currentScope + "::" + functionName;
        functionCallStack.push_back(newScope);
        
        // Enter new function scope
        inFunction = true;
        localVariables.clear(); // Clear local variables for new function scope
        declaredGlobal.clear(); // Clear global declarations for new function
        declaredLocal.clear(); // Clear local declarations for new function
        
        // Set up function prologue: register parameters and move from calling convention registers
        assembly << "    # Function prologue: setting up parameters\n";
        
        // Register each parameter as a local variable with proper stack slot
        const std::string callingConventionRegs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        
        for (size_t i = 0; i < func->parameters.size(); i++) {
            const auto& param = func->parameters[i];
            
            // Allocate stack slot for parameter
            stackOffset += 8;
            VariableInfo paramInfo;
            paramInfo.stackOffset = stackOffset;
            paramInfo.type = param.type.toString(); // Convert Type to string
            paramInfo.isGlobal = false;
            paramInfo.isConstant = false;
            
            // Register parameter in local variables
            localVariables[param.name] = paramInfo;
            
            // Move parameter value from calling convention register to stack slot
            if (i < 6) {
                // First 6 parameters are passed in registers
                assembly << "    mov " << callingConventionRegs[i] << ", -" << paramInfo.stackOffset << "(%rbp)  # param " << param.name << " from " << callingConventionRegs[i] << "\n";
            } else {
                // Additional parameters are passed on stack (simplified - would need proper stack offset calculation)
                assembly << "    # Note: Parameter " << param.name << " beyond register capacity - would be on stack\n";
                assembly << "    mov $0, -" << paramInfo.stackOffset << "(%rbp)  # placeholder for stack parameter " << param.name << "\n";
            }
            
            assembly << "    # Parameter " << param.name << " (type: " << paramInfo.type << ") at stack offset -" << paramInfo.stackOffset << "\n";
        }
        
        // Execute function body
        if (func->isSingleExpression) {
            func->expression->accept(*this);
        } else {
            for (auto& stmt : func->body) {
                stmt->accept(*this);
            }
        }
        
        // Exit function scope - restore previous state and pop call stack
        functionCallStack.pop_back();
        inFunction = wasInFunction;
        localVariables = savedLocalVars; // Restore previous local scope
        declaredGlobal = savedDeclaredGlobal;
        declaredLocal = savedDeclaredLocal;
        stackOffset = savedStackOffset; // Restore stack offset
    }
    
    FunctionDeclaration* findFunction(const std::string& name) {
        // Implement Python-style function scoping: look in current scope, then parent scopes, then global
        
        // Start from current function context (if any)
        std::string currentScope = "";
        if (!functionCallStack.empty()) {
            currentScope = functionCallStack.back();
        }
        
        // Search in current scope hierarchy (from innermost to outermost)
        std::string searchScope = currentScope;
        while (true) {
            auto scopeIt = functionScopes.find(searchScope);
            if (scopeIt != functionScopes.end()) {
                auto funcIt = scopeIt->second.functions.find(name);
                if (funcIt != scopeIt->second.functions.end()) {
                    return funcIt->second;
                }
            }
            
            // Move to parent scope
            if (searchScope.empty()) {
                break; // Already at global scope
            }
            
            // Extract parent scope (remove last "::function" part)
            size_t pos = searchScope.find_last_of("::");
            if (pos == std::string::npos) {
                searchScope = ""; // Move to global scope
            } else {
                searchScope = searchScope.substr(0, pos-1); // Remove "::function" part
            }
        }
        
        return nullptr; // Function not found in any accessible scope
    }
    
    void visit(VariableDeclaration& node) override {
        assembly << "    # Variable: " << node.name << "\n";
        
        // Check if this is an assignment to an existing const variable
        if (!node.isConstant && constantVariables.count(node.name)) {
            throw std::runtime_error("Error: You are trying to change the value of a constant variable '" + node.name + "'");
        }
        
        if (node.initializer) {
            // Determine variable type from initializer
            std::string varType = "unknown";
            if (auto intLit = dynamic_cast<IntLiteral*>(node.initializer.get())) {
                varType = "int";
            } else if (auto strLit = dynamic_cast<StringLiteral*>(node.initializer.get())) {
                varType = "string";
            } else if (auto boolLit = dynamic_cast<BoolLiteral*>(node.initializer.get())) {
                varType = "bool";
            } else if (auto floatLit = dynamic_cast<FloatLiteral*>(node.initializer.get())) {
                varType = "float";
            } else if (auto listLit = dynamic_cast<ListLiteral*>(node.initializer.get())) {
                varType = "list";
            } else if (auto id = dynamic_cast<Identifier*>(node.initializer.get())) {
                // Variable assignment: copy type from source variable
                auto varInfo = lookupVariable(id->name);
                if (varInfo != nullptr) {
                    varType = varInfo->type;
                }
            } else if (auto binExpr = dynamic_cast<BinaryExpression*>(node.initializer.get())) {
                // Binary expression: infer type from operands
                // For arithmetic operations, the result is typically int
                switch (binExpr->op) {
                    case BinaryOp::ADD:
                    case BinaryOp::SUB:
                    case BinaryOp::MUL:
                    case BinaryOp::DIV:
                    case BinaryOp::MOD:
                    case BinaryOp::FLOOR_DIV:
                    case BinaryOp::POWER:
                        // Check if either operand is a float
                        if (isFloatExpression(binExpr->left.get()) || isFloatExpression(binExpr->right.get())) {
                            varType = "float";
                        } else {
                            varType = "int";
                        }
                        break;
                    case BinaryOp::EQ:
                    case BinaryOp::NE:
                    case BinaryOp::LT:
                    case BinaryOp::LE:
                    case BinaryOp::GT:
                    case BinaryOp::GE:
                    case BinaryOp::AND:
                    case BinaryOp::OR:
                        varType = "bool";  // Comparison/logical operations result in bool
                        break;
                    default:
                        varType = "int";  // Default to int for unknown operations
                        break;
                }
            } else if (auto funcCall = dynamic_cast<FunctionCall*>(node.initializer.get())) {
                // Function call: infer return type based on function name
                if (funcCall->name == "input") {
                    varType = "string";  // input() returns a string
                } else if (funcCall->name == "len") {
                    varType = "int";     // len() returns an integer
                } else if (funcCall->name == "dtype") {
                    varType = "string";  // dtype() returns a string representation
                } else {
                    // For user-defined functions, we don't know the return type yet
                    // Default to unknown and let runtime handle it
                    varType = "string";  // Most user functions likely return strings or can be treated as such
                }
            }
            
            // Check if variable already exists - if so, treat as reassignment
            VariableInfo* existingVar = lookupVariable(node.name);
            if (existingVar) {
                // Variable exists - treat as reassignment, don't allocate new slot
                if (node.isConstant && !existingVar->isConstant) {
                    // Trying to make existing variable const - not allowed
                    throw std::runtime_error("Error: Cannot make existing variable '" + node.name + "' constant");
                }
                if (existingVar->isConstant) {
                    throw std::runtime_error("Error: You are trying to change the value of a constant variable '" + node.name + "'");
                }
                // Update type if needed, but keep existing slot and scope
                existingVar->type = varType;
            } else {
                // Variable doesn't exist - create new variable
                // Python-style scoping rules - PRE-DECLARE variable before evaluating initializer
                if (declaredGlobal.count(node.name) || (!inFunction)) {
                    // Explicitly declared global OR not in function - use global scope
                    stackOffset += 8;
                    VariableInfo varInfo;
                    varInfo.stackOffset = stackOffset;
                    varInfo.type = varType;
                    varInfo.isGlobal = true;
                    varInfo.isConstant = node.isConstant;
                    globalVariables[node.name] = varInfo;
                    
                    if (node.isConstant) {
                        constantVariables.insert(node.name);
                    }
                } else {
                    // In function and not declared global - create local variable
                    stackOffset += 8;
                    VariableInfo varInfo;
                    varInfo.stackOffset = stackOffset;
                    varInfo.type = varType;
                    varInfo.isGlobal = false;
                    varInfo.isConstant = node.isConstant;
                    localVariables[node.name] = varInfo;
                    
                    if (node.isConstant) {
                        constantVariables.insert(node.name);
                    }
                }
            }
            
            // Now evaluate initializer - variable is already declared
            node.initializer->accept(*this);
            
            // Store the result in the pre-allocated variable slot using recorded offset
            VariableInfo* varInfo = lookupVariable(node.name);
            if (varInfo != nullptr) {
                assembly << "    mov %rax, -" << varInfo->stackOffset << "(%rbp)  # store " << (varInfo->isGlobal ? "global" : "local") << " " << node.name << "\n";
            }
        }
    }
    
    VariableInfo* lookupVariable(const std::string& name) {
        // Python-style variable lookup: local scope first, then global scope
        if (inFunction) {
            auto localIt = localVariables.find(name);
            if (localIt != localVariables.end()) {
                return &localIt->second;
            }
        }
        
        auto globalIt = globalVariables.find(name);
        if (globalIt != globalVariables.end()) {
            return &globalIt->second;
        }
        
        return nullptr; // Variable not found
    }
    
    void visit(ExpressionStatement& node) override {
        node.expression->accept(*this);
    }
    
    void visit(FunctionCall& node) override {
        // Handle built-in type conversion functions
        if (node.name == "str") {
            if (node.arguments.size() != 1) {
                throw std::runtime_error("str() function requires exactly 1 argument");
            }
            assembly << "    # str() type conversion function call\n";
            
            // Evaluate the argument
            node.arguments[0]->accept(*this);
            
            // Determine the type of the argument and call appropriate runtime helper
            auto argExpr = node.arguments[0].get();
            if (auto intLit = dynamic_cast<IntLiteral*>(argExpr)) {
                assembly << "    mov %rax, %rdi  # int argument\n";
                assembly << "    call __orion_int_to_string\n";
            } else if (auto floatLit = dynamic_cast<FloatLiteral*>(argExpr)) {
                assembly << "    movq %rax, %xmm0  # float argument\n";
                assembly << "    call __orion_float_to_string\n";
            } else if (auto boolLit = dynamic_cast<BoolLiteral*>(argExpr)) {
                assembly << "    mov %rax, %rdi  # bool argument\n";
                assembly << "    call __orion_bool_to_string\n";
            } else if (auto strLit = dynamic_cast<StringLiteral*>(argExpr)) {
                // String to string is identity - result already in %rax
                assembly << "    # String to string conversion (identity)\n";
            } else if (auto id = dynamic_cast<Identifier*>(argExpr)) {
                // Variable - determine type from variable info
                auto varInfo = lookupVariable(id->name);
                if (varInfo) {
                    if (varInfo->type == "int") {
                        assembly << "    mov %rax, %rdi  # int variable\n";
                        assembly << "    call __orion_int_to_string\n";
                    } else if (varInfo->type == "float") {
                        assembly << "    movq %rax, %xmm0  # float variable\n";
                        assembly << "    call __orion_float_to_string\n";
                    } else if (varInfo->type == "bool") {
                        assembly << "    mov %rax, %rdi  # bool variable\n";
                        assembly << "    call __orion_bool_to_string\n";
                    } else if (varInfo->type == "string") {
                        // String to string is identity
                        assembly << "    # String variable to string conversion (identity)\n";
                    }
                }
            } else if (auto funcCall = dynamic_cast<FunctionCall*>(argExpr)) {
                // Handle function call arguments to str()
                if (funcCall->name == "flt") {
                    assembly << "    movq %rax, %xmm0  # flt() result as float\n";
                    assembly << "    call __orion_float_to_string\n";
                } else if (funcCall->name == "int") {
                    assembly << "    mov %rax, %rdi  # int() result as integer\n";
                    assembly << "    call __orion_int_to_string\n";
                } else {
                    // Other function calls default to int for now
                    assembly << "    mov %rax, %rdi  # other function result\n";
                    assembly << "    call __orion_int_to_string\n";
                }
            } else {
                // For other complex expressions, default to int conversion
                assembly << "    mov %rax, %rdi  # complex expression argument\n";
                assembly << "    call __orion_int_to_string  # Default to int conversion\n";
            }
            return;
        } else if (node.name == "int") {
            if (node.arguments.size() != 1) {
                throw std::runtime_error("int() function requires exactly 1 argument");
            }
            assembly << "    # int() type conversion function call\n";
            
            // Evaluate the argument
            node.arguments[0]->accept(*this);
            
            // Determine the type of the argument and call appropriate runtime helper
            auto argExpr = node.arguments[0].get();
            if (auto intLit = dynamic_cast<IntLiteral*>(argExpr)) {
                // Int to int is identity - result already in %rax
                assembly << "    # Int to int conversion (identity)\n";
            } else if (auto floatLit = dynamic_cast<FloatLiteral*>(argExpr)) {
                assembly << "    movq %rax, %xmm0  # float argument\n";
                assembly << "    call __orion_float_to_int\n";
            } else if (auto boolLit = dynamic_cast<BoolLiteral*>(argExpr)) {
                assembly << "    mov %rax, %rdi  # bool argument\n";
                assembly << "    call __orion_bool_to_int\n";
            } else if (auto strLit = dynamic_cast<StringLiteral*>(argExpr)) {
                assembly << "    mov %rax, %rdi  # string argument\n";
                assembly << "    call __orion_string_to_int\n";
            } else if (auto id = dynamic_cast<Identifier*>(argExpr)) {
                // Variable - determine type from variable info
                auto varInfo = lookupVariable(id->name);
                if (varInfo) {
                    if (varInfo->type == "int") {
                        assembly << "    # Int variable to int conversion (identity)\n";
                    } else if (varInfo->type == "float") {
                        assembly << "    movq %rax, %xmm0  # float variable\n";
                        assembly << "    call __orion_float_to_int\n";
                    } else if (varInfo->type == "bool") {
                        assembly << "    mov %rax, %rdi  # bool variable\n";
                        assembly << "    call __orion_bool_to_int\n";
                    } else if (varInfo->type == "string") {
                        assembly << "    mov %rax, %rdi  # string variable\n";
                        assembly << "    call __orion_string_to_int\n";
                    }
                }
            } else {
                // For complex expressions, default to int conversion
                assembly << "    mov %rax, %rdi  # complex expression argument\n";
                assembly << "    call __orion_int_to_int  # Default to int identity\n";
            }
            return;
        } else if (node.name == "flt") {
            if (node.arguments.size() != 1) {
                throw std::runtime_error("flt() function requires exactly 1 argument");
            }
            assembly << "    # flt() type conversion function call\n";
            
            // Evaluate the argument
            node.arguments[0]->accept(*this);
            
            // Determine the type of the argument and call appropriate runtime helper
            auto argExpr = node.arguments[0].get();
            if (auto intLit = dynamic_cast<IntLiteral*>(argExpr)) {
                assembly << "    mov %rax, %rdi  # int argument\n";
                assembly << "    call __orion_int_to_float\n";
            } else if (auto floatLit = dynamic_cast<FloatLiteral*>(argExpr)) {
                // Float to float is identity - result already in %rax
                assembly << "    # Float to float conversion (identity)\n";
            } else if (auto boolLit = dynamic_cast<BoolLiteral*>(argExpr)) {
                assembly << "    mov %rax, %rdi  # bool argument\n";
                assembly << "    call __orion_bool_to_float\n";
            } else if (auto strLit = dynamic_cast<StringLiteral*>(argExpr)) {
                assembly << "    mov %rax, %rdi  # string argument\n";
                assembly << "    call __orion_string_to_float\n";
            } else if (auto id = dynamic_cast<Identifier*>(argExpr)) {
                // Variable - determine type from variable info
                auto varInfo = lookupVariable(id->name);
                if (varInfo) {
                    if (varInfo->type == "int") {
                        assembly << "    mov %rax, %rdi  # int variable\n";
                        assembly << "    call __orion_int_to_float\n";
                    } else if (varInfo->type == "float") {
                        assembly << "    # Float variable to float conversion (identity)\n";
                    } else if (varInfo->type == "bool") {
                        assembly << "    mov %rax, %rdi  # bool variable\n";
                        assembly << "    call __orion_bool_to_float\n";
                    } else if (varInfo->type == "string") {
                        assembly << "    mov %rax, %rdi  # string variable\n";
                        assembly << "    call __orion_string_to_float\n";
                    }
                }
            } else {
                // For complex expressions, default to int to float conversion
                assembly << "    mov %rax, %rdi  # complex expression argument\n";
                assembly << "    call __orion_int_to_float  # Default to int to float\n";
            }
            return;
        }
        
        // Handle built-in list functions
        if (node.name == "len") {
            if (node.arguments.size() != 1) {
                throw std::runtime_error("len() function requires exactly 1 argument");
            }
            assembly << "    # len() function call\n";
            
            // Check if the argument is a range() function call
            if (auto funcCall = dynamic_cast<FunctionCall*>(node.arguments[0].get())) {
                if (funcCall->name == "range") {
                    // This is a range object - call range_len
                    node.arguments[0]->accept(*this);  // Evaluate range argument
                    assembly << "    mov %rax, %rdi  # Range pointer as argument\n";
                    assembly << "    call range_len  # Get range length\n";
                    // Result in %rax
                    return;
                }
            }
            
            // Default to list behavior for other cases
            node.arguments[0]->accept(*this);  // Evaluate list argument
            assembly << "    mov %rax, %rdi  # List pointer as argument\n";
            assembly << "    call list_len  # Get list length\n";
            // Result in %rax
            return;
        }
        
        if (node.name == "append") {
            if (node.arguments.size() != 2) {
                throw std::runtime_error("append() function requires exactly 2 arguments (list, element)");
            }
            assembly << "    # append() function call\n";
            
            // Evaluate list argument
            node.arguments[0]->accept(*this);
            assembly << "    mov %rax, %rdi  # List pointer as first argument\n";
            assembly << "    push %rdi  # Save list pointer\n";
            
            // Evaluate element argument
            node.arguments[1]->accept(*this);
            assembly << "    mov %rax, %rsi  # Element value as second argument\n";
            assembly << "    pop %rdi  # Restore list pointer\n";
            
            assembly << "    call list_append  # Append element to list\n";
            // append returns void, so no return value
            return;
        }
        
        if (node.name == "pop") {
            if (node.arguments.size() != 1) {
                throw std::runtime_error("pop() function requires exactly 1 argument");
            }
            assembly << "    # pop() function call\n";
            node.arguments[0]->accept(*this);  // Evaluate list argument
            assembly << "    mov %rax, %rdi  # List pointer as argument\n";
            assembly << "    call list_pop  # Pop last element\n";
            // Result (popped element) in %rax
            return;
        }
        
        if (node.name == "range") {
            // Handle range() function calls with 1-3 arguments
            if (node.arguments.size() < 1 || node.arguments.size() > 3) {
                throw std::runtime_error("range() function requires 1, 2, or 3 arguments");
            }
            
            assembly << "    # range() function call\n";
            
            if (node.arguments.size() == 1) {
                // range(stop) - start=0, step=1
                node.arguments[0]->accept(*this);  // Evaluate stop argument
                assembly << "    mov %rax, %rdi  # Stop value as argument\n";
                assembly << "    call range_new_stop  # Create range with stop only\n";
            } else if (node.arguments.size() == 2) {
                // range(start, stop) - step=1
                node.arguments[0]->accept(*this);  // Evaluate start argument
                assembly << "    mov %rax, %rdi  # Start value as first argument\n";
                assembly << "    push %rdi  # Save start value\n";
                
                node.arguments[1]->accept(*this);  // Evaluate stop argument
                assembly << "    mov %rax, %rsi  # Stop value as second argument\n";
                assembly << "    pop %rdi  # Restore start value\n";
                
                assembly << "    call range_new_start_stop  # Create range with start and stop\n";
            } else {
                // range(start, stop, step)
                node.arguments[0]->accept(*this);  // Evaluate start argument
                assembly << "    mov %rax, %rdi  # Start value as first argument\n";
                assembly << "    push %rdi  # Save start value\n";
                
                node.arguments[1]->accept(*this);  // Evaluate stop argument
                assembly << "    mov %rax, %rsi  # Stop value as second argument\n";
                assembly << "    push %rsi  # Save stop value\n";
                
                node.arguments[2]->accept(*this);  // Evaluate step argument
                assembly << "    mov %rax, %rdx  # Step value as third argument\n";
                assembly << "    pop %rsi  # Restore stop value\n";
                assembly << "    pop %rdi  # Restore start value\n";
                
                assembly << "    call range_new  # Create range with start, stop, and step\n";
            }
            // Result (range pointer) in %rax
            return;
        }
        
        if (node.name == "out") {
            // Handle out() function calls
            if (!node.arguments.empty()) {
                auto& arg = node.arguments[0];
                
                // Check if the argument is a special function call
                if (auto funcCall = dynamic_cast<FunctionCall*>(arg.get())) {
                    // Handle built-in type conversion functions
                    if (funcCall->name == "str") {
                        assembly << "    # Call out() with str() result\n";
                        funcCall->accept(*this);  // This calls str() and puts result in %rax
                        assembly << "    mov %rax, %rsi  # String pointer as argument\n";
                        assembly << "    mov $format_str, %rdi  # Use string format\n";
                        assembly << "    xor %rax, %rax\n";
                        assembly << "    call printf\n";
                        return;
                    } else if (funcCall->name == "int") {
                        assembly << "    # Call out() with int() result\n";
                        funcCall->accept(*this);  // This calls int() and puts result in %rax
                        assembly << "    mov %rax, %rsi  # Integer value as argument\n";
                        assembly << "    mov $format_int, %rdi  # Use integer format\n";
                        assembly << "    xor %rax, %rax\n";
                        assembly << "    call printf\n";
                        return;
                    } else if (funcCall->name == "flt") {
                        assembly << "    # Call out() with flt() result\n";
                        funcCall->accept(*this);  // This calls flt() and puts result in %rax
                        assembly << "    movq %rax, %xmm0  # Float value to XMM register\n";
                        assembly << "    mov $format_float, %rdi  # Use float format\n";
                        assembly << "    mov $1, %rax  # Number of vector registers used\n";
                        assembly << "    call printf\n";
                        return;
                    } else if (funcCall->name == "dtype" && !funcCall->arguments.empty()) {
                        // Handle dtype() call inside out()
                        auto dtypeArg = funcCall->arguments[0].get();
                        if (auto id = dynamic_cast<Identifier*>(dtypeArg)) {
                            auto varIt = lookupVariable(id->name);
                            if (varIt != nullptr) {
                                assembly << "    # Call out(dtype(" << id->name << "))\n";
                                std::string dtypeLabel;
                                if (varIt->type == "int") {
                                    dtypeLabel = "dtype_int";
                                } else if (varIt->type == "string") {
                                    dtypeLabel = "dtype_string";
                                } else if (varIt->type == "bool") {
                                    dtypeLabel = "dtype_bool";
                                } else if (varIt->type == "float") {
                                    dtypeLabel = "dtype_float";
                                } else if (varIt->type == "list") {
                                    dtypeLabel = "dtype_list";
                                } else {
                                    dtypeLabel = "dtype_unknown";
                                }
                                assembly << "    mov $" << dtypeLabel << ", %rsi\n";
                                assembly << "    mov $format_str, %rdi\n";
                                assembly << "    xor %rax, %rax\n";
                                assembly << "    call printf\n";
                            } else {
                                throw std::runtime_error("Line " + std::to_string(id->line) + ": Error: Undefined variable '" + id->name + "'");
                            }
                        }
                        return;
                    }
                }
                
                // Check the type of argument to determine format
                if (auto intLit = dynamic_cast<IntLiteral*>(arg.get())) {
                    assembly << "    # Call out() with integer\n";
                    assembly << "    mov $" << intLit->value << ", %rsi\n";
                    assembly << "    mov $format_int, %rdi\n";
                    assembly << "    xor %rax, %rax\n";
                    assembly << "    call printf\n";
                } else if (auto strLit = dynamic_cast<StringLiteral*>(arg.get())) {
                    int index = addStringLiteral(strLit->value);
                    assembly << "    # Call out() with string\n";
                    assembly << "    mov $str_" << index << ", %rsi\n";
                    assembly << "    mov $format_str, %rdi\n";
                    assembly << "    xor %rax, %rax\n";
                    assembly << "    call printf\n";
                } else if (auto id = dynamic_cast<Identifier*>(arg.get())) {
                    // Variable reference - use correct format based on type
                    auto it = lookupVariable(id->name);
                    if (it != nullptr) {
                        assembly << "    # Call out() with variable: " << id->name << " (type: " << it->type << ")\n";
                        assembly << "    mov -" << it->stackOffset << "(%rbp), %rsi\n";
                        
                        if (it->type == "int") {
                            assembly << "    mov $format_int, %rdi\n";
                            assembly << "    xor %rax, %rax\n";
                        } else if (it->type == "bool") {
                            assembly << "    mov $format_str, %rdi\n";
                            assembly << "    xor %rax, %rax\n";
                        } else if (it->type == "float") {
                            assembly << "    movq -" << it->stackOffset << "(%rbp), %xmm0\n";  // Load float into XMM register  
                            assembly << "    mov $format_float, %rdi\n";
                            assembly << "    mov $1, %rax\n";  // Number of vector registers used
                        } else {
                            assembly << "    mov $format_str, %rdi\n";
                            assembly << "    xor %rax, %rax\n";
                        }
                        
                        assembly << "    call printf\n";
                    } else {
                        throw std::runtime_error("Error: Undefined variable '" + id->name + "'");
                    }
                } else if (auto boolLit = dynamic_cast<BoolLiteral*>(arg.get())) {
                    // Boolean literal - output as string
                    assembly << "    # Call out() with boolean literal\n";
                    assembly << "    mov $" << (boolLit->value ? "str_true" : "str_false") << ", %rsi\n";
                    assembly << "    mov $format_str, %rdi\n";
                    assembly << "    xor %rax, %rax\n";
                    assembly << "    call printf\n";
                } else if (auto interpolated = dynamic_cast<InterpolatedString*>(arg.get())) {
                    // Handle interpolated string - evaluate it and treat result as string
                    assembly << "    # Call out() with interpolated string\n";
                    arg->accept(*this);  // This calls our InterpolatedString visitor
                    assembly << "    mov %rax, %rsi  # String pointer from interpolation result\n";
                    assembly << "    mov $format_str, %rdi  # Use string format\n";
                    assembly << "    xor %rax, %rax\n";
                    assembly << "    call printf\n";
                } else {
                    // Generic expression (like arithmetic operations or comparisons)
                    // Check if the expression result is a float
                    bool isFloatResult = false;
                    bool isComparisonResult = false;
                    
                    if (auto binExpr = dynamic_cast<BinaryExpression*>(arg.get())) {
                        // Check if it's a comparison or logical operation
                        if (binExpr->op == BinaryOp::EQ || binExpr->op == BinaryOp::NE ||
                            binExpr->op == BinaryOp::LT || binExpr->op == BinaryOp::LE ||
                            binExpr->op == BinaryOp::GT || binExpr->op == BinaryOp::GE ||
                            binExpr->op == BinaryOp::AND || binExpr->op == BinaryOp::OR) {
                            isComparisonResult = true;
                        } else {
                            isFloatResult = isFloatExpression(binExpr->left.get()) || isFloatExpression(binExpr->right.get());
                        }
                    } else if (auto unaryExpr = dynamic_cast<UnaryExpression*>(arg.get())) {
                        // Check if it's a NOT operation (returns string)
                        if (unaryExpr->op == UnaryOp::NOT) {
                            isComparisonResult = true;
                        }
                    } else if (auto floatLit = dynamic_cast<FloatLiteral*>(arg.get())) {
                        isFloatResult = true;
                    }
                    
                    arg->accept(*this);
                    assembly << "    # Call out() with expression result\n";
                    
                    if (isComparisonResult) {
                        // Comparison results are string addresses (str_true/str_false)
                        assembly << "    mov %rax, %rsi\n";
                        assembly << "    mov $format_str, %rdi\n";  // Use string format for comparison results
                        assembly << "    xor %rax, %rax\n";
                    } else if (isFloatResult) {
                        assembly << "    movq %rax, %xmm0  # Load float result into XMM register\n";
                        assembly << "    mov $format_float, %rdi\n";
                        assembly << "    mov $1, %rax  # Number of vector registers used\n";
                    } else {
                        assembly << "    mov %rax, %rsi\n";
                        assembly << "    mov $format_int, %rdi\n";  // Use integer format for computed results
                        assembly << "    xor %rax, %rax\n";
                    }
                    assembly << "    call printf\n";
                }
            }
        } else if (node.name == "input") {
            // Handle input() function calls
            assembly << "    # input() function call\n";
            
            if (node.arguments.empty()) {
                // input() without prompt
                assembly << "    call orion_input  # Read input from stdin\n";
                assembly << "    # String address returned in %rax\n";
            } else if (node.arguments.size() == 1) {
                // input("prompt") with prompt
                auto& promptArg = node.arguments[0];
                
                if (auto strLit = dynamic_cast<StringLiteral*>(promptArg.get())) {
                    // String literal prompt
                    int index = addStringLiteral(strLit->value);
                    assembly << "    mov $str_" << index << ", %rdi  # Prompt string\n";
                    assembly << "    call orion_input_prompt  # Display prompt and read input\n";
                    assembly << "    # String address returned in %rax\n";
                } else if (auto id = dynamic_cast<Identifier*>(promptArg.get())) {
                    // Variable prompt
                    auto varInfo = lookupVariable(id->name);
                    if (varInfo && varInfo->type == "string") {
                        assembly << "    mov -" << varInfo->stackOffset << "(%rbp), %rdi  # Prompt from variable\n";
                        assembly << "    call orion_input_prompt  # Display prompt and read input\n";
                        assembly << "    # String address returned in %rax\n";
                    } else {
                        throw std::runtime_error("Error: input() prompt must be a string");
                    }
                } else {
                    throw std::runtime_error("Error: input() prompt must be a string literal or variable");
                }
            } else {
                throw std::runtime_error("Error: input() function takes 0 or 1 argument");
            }
            return;
        } else if (node.name == "dtype") {
            // Handle standalone dtype() calls (though typically used inside out())
            if (!node.arguments.empty()) {
                auto& arg = node.arguments[0];
                if (auto id = dynamic_cast<Identifier*>(arg.get())) {
                    auto varIt = lookupVariable(id->name);
                    if (varIt != nullptr) {
                        assembly << "    # dtype(" << id->name << ") - type: " << varIt->type << "\n";
                        // For standalone dtype(), we could return a type indicator
                        // For now, just put the type string address in %rax
                        std::string dtypeLabel;
                        if (varIt->type == "int") {
                            dtypeLabel = "dtype_int";
                        } else if (varIt->type == "string") {
                            dtypeLabel = "dtype_string";
                        } else if (varIt->type == "bool") {
                            dtypeLabel = "dtype_bool";
                        } else if (varIt->type == "float") {
                            dtypeLabel = "dtype_float";
                        } else {
                            dtypeLabel = "dtype_unknown";
                        }
                        assembly << "    mov $" << dtypeLabel << ", %rax\n";
                    } else {
                        throw std::runtime_error("Line " + std::to_string(id->line) + ": Error: Undefined variable '" + id->name + "'");
                    }
                }
            }
        } else {
            // Handle user-defined function calls - generate proper assembly
            assembly << "    # User-defined function call: " << node.name << "\n";
            
            // Prepare arguments in calling convention registers
            const std::string callingConventionRegs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
            
            for (size_t i = 0; i < node.arguments.size() && i < 6; i++) {
                assembly << "    # Preparing argument " << i << "\n";
                node.arguments[i]->accept(*this);  // Result in %rax
                assembly << "    mov %rax, " << callingConventionRegs[i] << "  # Arg " << i << " to " << callingConventionRegs[i] << "\n";
            }
            
            // Generate the function call with correct label name
            std::string callLabel = (node.name == "main") ? "fn_main" : node.name;
            assembly << "    call " << callLabel << "\n";
        }
    }
    
    void visit(BinaryExpression& node) override {
        // Check for list operations first
        if (node.op == BinaryOp::ADD) {
            // Use type inference for robust two-sided validation
            ExprKind leftKind = inferExprKind(node.left.get());
            ExprKind rightKind = inferExprKind(node.right.get());
            
            // If either operand is a list, require both to be lists
            if (leftKind == ExprKind::LIST || rightKind == ExprKind::LIST) {
                if (leftKind != ExprKind::LIST || rightKind != ExprKind::LIST) {
                    throw std::runtime_error("Error: Cannot concatenate list with non-list. Both operands of '+' must be lists.");
                }
                
                // This is list concatenation: list + list
                assembly << "    # List concatenation: list + list\n";
                
                // Evaluate left list
                node.left->accept(*this);
                assembly << "    mov %rax, %rdi  # First list as first argument\n";
                assembly << "    push %rdi  # Save first list\n";
                
                // Evaluate right list
                node.right->accept(*this);
                assembly << "    mov %rax, %rsi  # Second list as second argument\n";
                assembly << "    pop %rdi  # Restore first list\n";
                
                assembly << "    call list_concat  # Concatenate lists\n";
                return;
            }
        }
        
        if (node.op == BinaryOp::MUL) {
            // Use type inference for robust validation
            ExprKind leftKind = inferExprKind(node.left.get());
            ExprKind rightKind = inferExprKind(node.right.get());
            
            // Check for list * int or int * list (valid repetition)
            if (leftKind == ExprKind::LIST && rightKind == ExprKind::INT) {
                // This is list * n
                assembly << "    # List repetition: list * n\n";
                
                // Evaluate list
                node.left->accept(*this);
                assembly << "    mov %rax, %rdi  # List as first argument\n";
                assembly << "    push %rdi  # Save list\n";
                
                // Evaluate repeat count
                node.right->accept(*this);
                assembly << "    mov %rax, %rsi  # Repeat count as second argument\n";
                assembly << "    pop %rdi  # Restore list\n";
                
                assembly << "    call list_repeat  # Repeat list\n";
                return;
            }
            
            if (leftKind == ExprKind::INT && rightKind == ExprKind::LIST) {
                // This is n * list
                assembly << "    # List repetition: n * list\n";
                
                // Evaluate repeat count
                node.left->accept(*this);
                assembly << "    mov %rax, %rsi  # Repeat count as second argument\n";
                assembly << "    push %rsi  # Save repeat count\n";
                
                // Evaluate list
                node.right->accept(*this);
                assembly << "    mov %rax, %rdi  # List as first argument\n";
                assembly << "    pop %rsi  # Restore repeat count\n";
                
                assembly << "    call list_repeat  # Repeat list\n";
                return;
            }
            
            // Check for invalid list operations
            if (leftKind == ExprKind::LIST || rightKind == ExprKind::LIST) {
                if (leftKind == ExprKind::LIST && rightKind == ExprKind::LIST) {
                    throw std::runtime_error("Error: Cannot multiply two lists. Use + for concatenation or * with an integer for repetition.");
                } else {
                    throw std::runtime_error("Error: List repetition requires an integer. Valid operations: list * int or int * list.");
                }
            }
        }
        
        // Check if both operands are strings for string comparison
        ExprKind leftKind = inferExprKind(node.left.get());
        ExprKind rightKind = inferExprKind(node.right.get());
        bool isStringComparison = (leftKind == ExprKind::STRING && rightKind == ExprKind::STRING);
        
        if (isStringComparison && (node.op == BinaryOp::EQ || node.op == BinaryOp::NE || 
                                  node.op == BinaryOp::LT || node.op == BinaryOp::LE || 
                                  node.op == BinaryOp::GT || node.op == BinaryOp::GE)) {
            // Handle string comparison operations
            assembly << "    # String comparison operation\n";
            
            // Evaluate left operand (string address)
            node.left->accept(*this);
            assembly << "    mov %rax, %rdi  # First string as first argument\n";
            assembly << "    push %rdi  # Save first string\n";
            
            // Evaluate right operand (string address)
            node.right->accept(*this);
            assembly << "    mov %rax, %rsi  # Second string as second argument\n";
            assembly << "    pop %rdi  # Restore first string\n";
            
            // Call strcmp to compare strings
            assembly << "    call strcmp  # Compare strings\n";
            
            // strcmp returns: 0 if equal, <0 if first < second, >0 if first > second
            switch (node.op) {
                case BinaryOp::EQ:
                    // Set %rax to 1 if equal (strcmp returned 0), otherwise 0
                    assembly << "    cmp $0, %eax  # Compare strcmp result with 0\n";
                    assembly << "    sete %al      # Set %al to 1 if equal, 0 if not\n";
                    assembly << "    movzx %al, %rax  # Zero-extend to full register\n";
                    break;
                case BinaryOp::NE:
                    // Set %rax to 1 if not equal (strcmp returned non-zero), otherwise 0
                    assembly << "    cmp $0, %eax  # Compare strcmp result with 0\n";
                    assembly << "    setne %al     # Set %al to 1 if not equal, 0 if equal\n";
                    assembly << "    movzx %al, %rax  # Zero-extend to full register\n";
                    break;
                case BinaryOp::LT:
                    assembly << "    test %rax, %rax\n";
                    assembly << "    js slt_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp slt_done_" << labelCounter << "\n";
                    assembly << "slt_true_" << labelCounter << ":" << "\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "slt_done_" << labelCounter << ":" << "\n";
                    labelCounter++;
                    break;
                case BinaryOp::LE:
                    assembly << "    test %rax, %rax\n";
                    assembly << "    jle sle_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp sle_done_" << labelCounter << "\n";
                    assembly << "sle_true_" << labelCounter << ":" << "\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "sle_done_" << labelCounter << ":" << "\n";
                    labelCounter++;
                    break;
                case BinaryOp::GT:
                    assembly << "    test %rax, %rax\n";
                    assembly << "    jg sgt_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp sgt_done_" << labelCounter << "\n";
                    assembly << "sgt_true_" << labelCounter << ":" << "\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "sgt_done_" << labelCounter << ":" << "\n";
                    labelCounter++;
                    break;
                case BinaryOp::GE:
                    assembly << "    test %rax, %rax\n";
                    assembly << "    jge sge_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp sge_done_" << labelCounter << "\n";
                    assembly << "sge_true_" << labelCounter << ":" << "\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "sge_done_" << labelCounter << ":" << "\n";
                    labelCounter++;
                    break;
                default:
                    break;
            }
            return;
        }
        
        // Check if either operand is a float
        bool leftIsFloat = isFloatExpression(node.left.get());
        bool rightIsFloat = isFloatExpression(node.right.get());
        bool isFloatOperation = leftIsFloat || rightIsFloat;
        
        if (isFloatOperation) {
            // Handle floating-point arithmetic
            assembly << "    # Floating-point binary operation\n";
            
            // Evaluate left operand
            node.left->accept(*this);
            if (leftIsFloat) {
                assembly << "    movq %rax, %xmm0  # Load float left operand\n";
            } else {
                assembly << "    cvtsi2sd %rax, %xmm0  # Convert int to float (left)\n";
            }
            assembly << "    subq $8, %rsp\n";
            assembly << "    movsd %xmm0, (%rsp)  # Save left operand on stack\n";
            
            // Evaluate right operand
            node.right->accept(*this);
            if (rightIsFloat) {
                assembly << "    movq %rax, %xmm1  # Load float right operand\n";
            } else {
                assembly << "    cvtsi2sd %rax, %xmm1  # Convert int to float (right)\n";
            }
            
            // Load left operand back
            assembly << "    movsd (%rsp), %xmm0  # Restore left operand\n";
            assembly << "    addq $8, %rsp\n";
            
            // Perform floating-point operation
            switch (node.op) {
                case BinaryOp::ADD:
                    assembly << "    addsd %xmm1, %xmm0  # Float addition\n";
                    break;
                case BinaryOp::SUB:
                    assembly << "    subsd %xmm1, %xmm0  # Float subtraction\n";
                    break;
                case BinaryOp::MUL:
                    assembly << "    mulsd %xmm1, %xmm0  # Float multiplication\n";
                    break;
                case BinaryOp::DIV:
                    assembly << "    divsd %xmm1, %xmm0  # Float division\n";
                    break;
                case BinaryOp::FLOOR_DIV:
                    // Floor division: divide then apply floor function
                    assembly << "    divsd %xmm1, %xmm0  # Float division\n";
                    assembly << "    # Apply floor function\n";
                    assembly << "    subq $8, %rsp  # Align stack\n";
                    assembly << "    movsd %xmm0, (%rsp)  # Save division result\n";
                    assembly << "    movsd (%rsp), %xmm0  # Load argument for floor\n";
                    assembly << "    call floor  # Call C library floor function\n";
                    assembly << "    addq $8, %rsp  # Restore stack\n";
                    break;
                case BinaryOp::MOD:
                    // Float modulo using fmod function
                    assembly << "    # Float modulo - save registers and call fmod\n";
                    assembly << "    subq $16, %rsp  # Align stack\n";
                    assembly << "    movsd %xmm0, (%rsp)  # Save first operand\n";
                    assembly << "    movsd %xmm1, 8(%rsp)  # Save second operand\n";
                    assembly << "    movsd (%rsp), %xmm0  # Load first arg for fmod\n";
                    assembly << "    movsd 8(%rsp), %xmm1  # Load second arg for fmod\n";
                    assembly << "    call fmod  # Call C library fmod function\n";
                    assembly << "    addq $16, %rsp  # Restore stack\n";
                    break;
                case BinaryOp::POWER:
                    // Float power using pow function
                    assembly << "    # Float power - save registers and call pow\n";
                    assembly << "    subq $16, %rsp  # Align stack\n";
                    assembly << "    movsd %xmm0, (%rsp)  # Save base\n";
                    assembly << "    movsd %xmm1, 8(%rsp)  # Save exponent\n";
                    assembly << "    movsd (%rsp), %xmm0  # Load base for pow\n";
                    assembly << "    movsd 8(%rsp), %xmm1  # Load exponent for pow\n";
                    assembly << "    call pow  # Call C library pow function\n";
                    assembly << "    addq $16, %rsp  # Restore stack\n";
                    break;
                case BinaryOp::EQ:
                    assembly << "    comisd %xmm1, %xmm0\n";
                    assembly << "    je feq_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp feq_done_" << labelCounter << "\n";
                    assembly << "feq_true_" << labelCounter << ":\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "feq_done_" << labelCounter << ":\n";
                    labelCounter++;
                    return;
                case BinaryOp::NE:
                    assembly << "    comisd %xmm1, %xmm0\n";
                    assembly << "    jne fne_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp fne_done_" << labelCounter << "\n";
                    assembly << "fne_true_" << labelCounter << ":\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "fne_done_" << labelCounter << ":\n";
                    labelCounter++;
                    return;
                case BinaryOp::LT:
                    assembly << "    comisd %xmm1, %xmm0\n";
                    assembly << "    jb flt_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp flt_done_" << labelCounter << "\n";
                    assembly << "flt_true_" << labelCounter << ":\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "flt_done_" << labelCounter << ":\n";
                    labelCounter++;
                    return;
                case BinaryOp::LE:
                    assembly << "    comisd %xmm1, %xmm0\n";
                    assembly << "    jbe fle_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp fle_done_" << labelCounter << "\n";
                    assembly << "fle_true_" << labelCounter << ":\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "fle_done_" << labelCounter << ":\n";
                    labelCounter++;
                    return;
                case BinaryOp::GT:
                    assembly << "    comisd %xmm1, %xmm0\n";
                    assembly << "    ja fgt_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp fgt_done_" << labelCounter << "\n";
                    assembly << "fgt_true_" << labelCounter << ":\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "fgt_done_" << labelCounter << ":\n";
                    labelCounter++;
                    return;
                case BinaryOp::GE:
                    assembly << "    comisd %xmm1, %xmm0\n";
                    assembly << "    jae fge_true_" << labelCounter << "\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "    jmp fge_done_" << labelCounter << "\n";
                    assembly << "fge_true_" << labelCounter << ":\n";
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "fge_done_" << labelCounter << ":\n";
                    labelCounter++;
                    return;
                default:
                    assembly << "    # Unsupported float operation - ERROR\n";
                    assembly << "    mov $0, %rax  # Return 0 for unsupported operations\n";
                    assembly << "    cvtsi2sd %rax, %xmm0  # Convert 0 to float\n";
                    break;
            }
            
            // Store result back to %rax as raw bits
            assembly << "    movq %xmm0, %rax  # Store float result\n";
        } else {
            // Handle integer arithmetic (original code)
            assembly << "    # Integer binary operation\n";
            
            // Evaluate left operand
            node.left->accept(*this);
            assembly << "    push %rax\n";
            
            // Evaluate right operand
            node.right->accept(*this);
            assembly << "    pop %rbx\n";
            
            // Perform operation
            switch (node.op) {
                case BinaryOp::ADD:
                    assembly << "    add %rbx, %rax\n";
                    break;
                case BinaryOp::SUB:
                    assembly << "    sub %rax, %rbx\n";
                    assembly << "    mov %rbx, %rax\n";
                    break;
                case BinaryOp::MUL:
                    assembly << "    imul %rbx, %rax\n";
                    break;
                case BinaryOp::DIV:
                    assembly << "    mov %rax, %rcx\n";
                    assembly << "    mov %rbx, %rax\n";
                    assembly << "    xor %rdx, %rdx\n";
                    assembly << "    idiv %rcx\n";
                    break;
                case BinaryOp::MOD:
                    assembly << "    mov %rax, %rcx\n";
                    assembly << "    mov %rbx, %rax\n";
                    assembly << "    xor %rdx, %rdx\n";
                    assembly << "    idiv %rcx\n";
                    assembly << "    mov %rdx, %rax\n";
                    break;
                case BinaryOp::FLOOR_DIV:
                    // Integer division (same as DIV for integers)
                    assembly << "    mov %rax, %rcx\n";
                    assembly << "    mov %rbx, %rax\n";
                    assembly << "    xor %rdx, %rdx\n";
                    assembly << "    idiv %rcx\n";
                    break;
                case BinaryOp::POWER:
                    // Simple power implementation for small integers
                    assembly << "    mov %rbx, %rcx  # base\n";
                    assembly << "    mov %rax, %rdx  # exponent\n";
                    assembly << "    mov $1, %rax    # result = 1\n";
                    assembly << "power_loop:\n";
                    assembly << "    test %rdx, %rdx\n";
                    assembly << "    jz power_done\n";
                    assembly << "    imul %rcx, %rax\n";
                    assembly << "    dec %rdx\n";
                    assembly << "    jmp power_loop\n";
                    assembly << "power_done:\n";
                    break;
                case BinaryOp::EQ:
                    assembly << "    cmp %rax, %rbx\n";
                    assembly << "    sete %al\n";
                    assembly << "    movzx %al, %rax\n";
                    break;
                case BinaryOp::NE:
                    assembly << "    cmp %rax, %rbx\n";
                    assembly << "    setne %al\n";
                    assembly << "    movzx %al, %rax\n";
                    break;
                case BinaryOp::LT:
                    assembly << "    cmp %rax, %rbx\n";
                    assembly << "    setl %al\n";
                    assembly << "    movzx %al, %rax\n";
                    break;
                case BinaryOp::LE:
                    assembly << "    cmp %rax, %rbx\n";
                    assembly << "    setle %al\n";
                    assembly << "    movzx %al, %rax\n";
                    break;
                case BinaryOp::GT:
                    assembly << "    cmp %rax, %rbx\n";
                    assembly << "    setg %al\n";
                    assembly << "    movzx %al, %rax\n";
                    break;
                case BinaryOp::GE:
                    assembly << "    cmp %rax, %rbx\n";
                    assembly << "    setge %al\n";
                    assembly << "    movzx %al, %rax\n";
                    break;
                case BinaryOp::AND:
                    // Logical AND: both operands must be truthy
                    // Left operand is falsy if it's 0 or str_false
                    assembly << "    cmp $0, %rbx\n";
                    assembly << "    je and_false_" << labelCounter << "\n";
                    assembly << "    cmp $str_false, %rbx\n";
                    assembly << "    je and_false_" << labelCounter << "\n";
                    // Left is truthy, check right operand
                    assembly << "    cmp $0, %rax\n";
                    assembly << "    je and_false_" << labelCounter << "\n";
                    assembly << "    cmp $str_false, %rax\n";
                    assembly << "    je and_false_" << labelCounter << "\n";
                    // Both are truthy
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "    jmp and_done_" << labelCounter << "\n";
                    assembly << "and_false_" << labelCounter << ":\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "and_done_" << labelCounter << ":\n";
                    labelCounter++;
                    break;
                case BinaryOp::OR:
                    // Logical OR: either operand can be truthy
                    // Check if left operand is truthy (not 0 and not str_false)
                    assembly << "    cmp $0, %rbx\n";
                    assembly << "    je or_check_right_" << labelCounter << "\n";
                    assembly << "    cmp $str_false, %rbx\n";
                    assembly << "    je or_check_right_" << labelCounter << "\n";
                    // Left is truthy
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "    jmp or_done_" << labelCounter << "\n";
                    assembly << "or_check_right_" << labelCounter << ":\n";
                    // Left is falsy, check right operand
                    assembly << "    cmp $0, %rax\n";
                    assembly << "    je or_false_" << labelCounter << "\n";
                    assembly << "    cmp $str_false, %rax\n";
                    assembly << "    je or_false_" << labelCounter << "\n";
                    // Right is truthy
                    assembly << "    mov $str_true, %rax\n";
                    assembly << "    jmp or_done_" << labelCounter << "\n";
                    assembly << "or_false_" << labelCounter << ":\n";
                    assembly << "    mov $str_false, %rax\n";
                    assembly << "or_done_" << labelCounter << ":\n";
                    labelCounter++;
                    break;
                case BinaryOp::ASSIGN:
                    // Chain assignment: a = (b = 5)
                    // Evaluate RHS -> store to LHS -> return value in RAX
                    assembly << "    # Chain assignment\n";
                    
                    // Evaluate RHS expression
                    node.right->accept(*this);
                    // Result is now in %rax - we'll use it directly
                    
                    // Extract LHS variable name and perform assignment
                    if (auto id = dynamic_cast<Identifier*>(node.left.get())) {
                        // Check if this is an assignment to an existing const variable
                        if (constantVariables.count(id->name)) {
                            throw std::runtime_error("Error: You are trying to change the value of a constant variable '" + id->name + "'");
                        }
                        
                        // Find or create variable
                        VariableInfo* varInfo = lookupVariable(id->name);
                        if (!varInfo) {
                            // Variable doesn't exist, create it
                            stackOffset += 8;
                            VariableInfo newVarInfo;
                            newVarInfo.stackOffset = stackOffset;
                            newVarInfo.type = "unknown";
                            newVarInfo.isGlobal = (!inFunction) || declaredGlobal.count(id->name);
                            newVarInfo.isConstant = false;
                            
                            if (inFunction && !declaredGlobal.count(id->name)) {
                                localVariables[id->name] = newVarInfo;
                                varInfo = &localVariables[id->name];
                            } else {
                                globalVariables[id->name] = newVarInfo;
                                varInfo = &globalVariables[id->name];
                            }
                        }
                        
                        // Store value to variable directly from %rax
                        assembly << "    mov %rax, -" << varInfo->stackOffset << "(%rbp)  # store " << id->name << "\n";
                    } else {
                        throw std::runtime_error("Error: Left side of assignment must be a variable");
                    }
                    
                    // RAX already contains the assigned value for chaining - no need to modify it
                    break;
                default:
                    assembly << "    # Unsupported binary operation\n";
                    break;
            }
        }
    }
    
    void visit(IntLiteral& node) override {
        assembly << "    mov $" << node.value << ", %rax\n";
    }
    
    void visit(StringLiteral& node) override {
        int index = addStringLiteral(node.value);
        assembly << "    mov $str_" << index << ", %rax\n";
    }
    
    void visit(InterpolatedString& node) override {
        assembly << "    # Interpolated string - proper implementation\n";
        
        if (node.parts.empty()) {
            // Empty interpolated string
            int index = addStringLiteral("");
            assembly << "    mov $str_" << index << ", %rax\n";
            return;
        }
        
        if (node.parts.size() == 1) {
            // Single part - handle directly
            const auto& part = node.parts[0];
            if (part.isExpression) {
                // Evaluate expression
                part.expression->accept(*this);
                
                // Convert expression result to string based on type
                if (auto id = dynamic_cast<Identifier*>(part.expression.get())) {
                    auto varInfo = lookupVariable(id->name);
                    if (varInfo) {
                        assembly << "    mov %rax, %rdi  # Expression result as argument\n";
                        if (varInfo->type == "int") {
                            assembly << "    call int_to_string  # Convert int to string\n";
                        } else if (varInfo->type == "float") {
                            assembly << "    call float_to_string  # Convert float to string\n";
                        } else if (varInfo->type == "bool") {
                            assembly << "    call bool_to_string  # Convert bool to string\n";
                        } else if (varInfo->type == "string") {
                            assembly << "    call string_to_string  # Copy string\n";
                        } else {
                            // Unknown type, treat as string pointer
                            assembly << "    call string_to_string  # Copy as string\n";
                        }
                    } else {
                        // Variable not found, treat as int by default
                        assembly << "    mov %rax, %rdi  # Expression result as argument\n";
                        assembly << "    call int_to_string  # Convert to string\n";
                    }
                } else {
                    // Direct expression (literal)
                    assembly << "    mov %rax, %rdi  # Expression result as argument\n";
                    if (dynamic_cast<IntLiteral*>(part.expression.get())) {
                        assembly << "    call int_to_string  # Convert int literal to string\n";
                    } else if (dynamic_cast<FloatLiteral*>(part.expression.get())) {
                        assembly << "    call float_to_string  # Convert float literal to string\n";
                    } else if (dynamic_cast<BoolLiteral*>(part.expression.get())) {
                        assembly << "    call bool_to_string  # Convert bool literal to string\n";
                    } else if (dynamic_cast<StringLiteral*>(part.expression.get())) {
                        assembly << "    call string_to_string  # Copy string literal\n";
                    } else {
                        // Default to int conversion for other expressions
                        assembly << "    call int_to_string  # Convert expression to string\n";
                    }
                }
            } else {
                // Single text part
                int textIndex = addStringLiteral(part.text);
                assembly << "    mov $str_" << textIndex << ", %rax\n";
            }
            return;
        }
        
        // Multiple parts - simplified concatenation approach
        assembly << "    # Multiple parts - simplified concatenation\n";
        
        // For now, let's implement a simpler approach that builds the string incrementally
        // Start with an empty result string
        assembly << "    mov $0, %r12  # Initialize result string to null\n";
        
        for (size_t i = 0; i < node.parts.size(); i++) {
            const auto& part = node.parts[i];
            
            assembly << "    # Process part " << i << "\n";
            
            if (part.isExpression) {
                assembly << "    # Expression part " << i << "\n";
                // Evaluate expression
                part.expression->accept(*this);
                
                // Convert to string based on type
                if (auto id = dynamic_cast<Identifier*>(part.expression.get())) {
                    auto varInfo = lookupVariable(id->name);
                    if (varInfo) {
                        assembly << "    mov %rax, %rdi\n";
                        if (varInfo->type == "int") {
                            assembly << "    call int_to_string\n";
                        } else if (varInfo->type == "float") {
                            assembly << "    call float_to_string\n";
                        } else if (varInfo->type == "bool") {
                            assembly << "    call bool_to_string\n";
                        } else if (varInfo->type == "string") {
                            assembly << "    call string_to_string\n";
                        } else {
                            assembly << "    call string_to_string\n";
                        }
                    } else {
                        assembly << "    mov %rax, %rdi\n";
                        assembly << "    call int_to_string\n";
                    }
                } else {
                    assembly << "    mov %rax, %rdi\n";
                    if (dynamic_cast<IntLiteral*>(part.expression.get())) {
                        assembly << "    call int_to_string\n";
                    } else if (dynamic_cast<FloatLiteral*>(part.expression.get())) {
                        assembly << "    call float_to_string\n";
                    } else if (dynamic_cast<BoolLiteral*>(part.expression.get())) {
                        assembly << "    call bool_to_string\n";
                    } else if (dynamic_cast<StringLiteral*>(part.expression.get())) {
                        assembly << "    call string_to_string\n";
                    } else {
                        assembly << "    call int_to_string\n";
                    }
                }
            } else {
                assembly << "    # Text part " << i << ": \"" << part.text << "\"\n";
                // Text part - get string literal address
                int textIndex = addStringLiteral(part.text);
                assembly << "    mov $str_" << textIndex << ", %rax\n";
                assembly << "    mov %rax, %rdi\n";
                assembly << "    call string_to_string  # Copy string literal\n";
            }
            
            // Now %rax contains the current part as a string
            if (i == 0) {
                // First part - just store it
                assembly << "    mov %rax, %r12  # Store first part\n";
            } else {
                // Subsequent parts - concatenate with previous result
                assembly << "    # Concatenate with previous result\n";
                assembly << "    push %rax  # Save current part\n";
                assembly << "    sub $16, %rsp  # Allocate space for 2 pointers\n";
                assembly << "    mov %r12, 0(%rsp)  # Store previous result\n";
                assembly << "    mov 16(%rsp), %rdi  # Get current part from stack\n";
                assembly << "    mov %rdi, 8(%rsp)  # Store current part\n";
                assembly << "    mov %rsp, %rdi  # Array of 2 string pointers\n";
                assembly << "    mov $2, %rsi  # Number of parts to concatenate\n";
                assembly << "    call string_concat_parts\n";
                assembly << "    add $16, %rsp  # Clean up array space\n";
                assembly << "    add $8, %rsp  # Clean up saved part\n";
                assembly << "    mov %rax, %r12  # Store new result\n";
            }
        }
        
        // Final result is in %r12
        assembly << "    mov %r12, %rax  # Move result to return register\n";
        assembly << "    # Multiple parts concatenation complete\n";
    }
    
    void visit(Identifier& node) override {
        // Python-style variable lookup: local scope first, then global scope
        VariableInfo* varInfo = lookupVariable(node.name);
        if (varInfo != nullptr) {
            assembly << "    mov -" << varInfo->stackOffset << "(%rbp), %rax  # load " << (varInfo->isGlobal ? "global" : "local") << " " << node.name << "\n";
        } else {
            std::string errorMsg = "Error: Undefined variable '" + node.name + "'";
            if (node.line > 0) {
                errorMsg = "Line " + std::to_string(node.line) + ": " + errorMsg;
            }
            throw std::runtime_error(errorMsg);
        }
    }
    
    void visit(TupleExpression& node) override {
        // For now, tuples just evaluate to the last element
        // In a full implementation, this would need proper tuple handling
        if (!node.elements.empty()) {
            node.elements.back()->accept(*this);
        }
    }
    
    void visit(TupleAssignment& node) override {
        // Handle tuple assignment like (a, b) = (c, d)
        // Algorithm: evaluate all RHS values first into stack temporaries,
        // then assign to LHS variables to prevent overwriting during swaps
        assembly << "    # Tuple assignment\n";
        
        if (node.targets.size() != node.values.size()) {
            throw std::runtime_error("Error: Tuple assignment mismatch - number of targets (" + 
                std::to_string(node.targets.size()) + ") doesn't match number of values (" + 
                std::to_string(node.values.size()) + ")");
        }
        
        // Check if any target variables are const before doing anything
        for (const auto& target : node.targets) {
            if (auto id = dynamic_cast<Identifier*>(target.get())) {
                if (constantVariables.count(id->name)) {
                    throw std::runtime_error("Error: You are trying to change the value of a constant variable '" + id->name + "'");
                }
            }
        }
        
        // Step 1: Evaluate all RHS values and push them onto the stack
        assembly << "    # Step 1: Evaluate all RHS values\n";
        for (size_t i = 0; i < node.values.size(); i++) {
            assembly << "    # Evaluating RHS value " << i << "\n";
            node.values[i]->accept(*this);
            assembly << "    push %rax  # Save RHS value " << i << " on stack\n";
        }
        
        // Step 2: Assign to LHS variables in reverse order (since stack is LIFO)
        assembly << "    # Step 2: Assign to LHS variables\n";
        for (int i = node.targets.size() - 1; i >= 0; i--) {
            assembly << "    # Assigning to LHS target " << i << "\n";
            assembly << "    pop %rax  # Get value " << i << " from stack\n";
            
            if (auto id = dynamic_cast<Identifier*>(node.targets[i].get())) {
                // Find or create variable
                VariableInfo* varInfo = lookupVariable(id->name);
                if (!varInfo) {
                    // Variable doesn't exist, create it
                    stackOffset += 8;
                    VariableInfo newVarInfo;
                    newVarInfo.stackOffset = stackOffset;
                    newVarInfo.type = "unknown";
                    newVarInfo.isGlobal = (!inFunction) || declaredGlobal.count(id->name);
                    newVarInfo.isConstant = false;
                    
                    if (inFunction && !declaredGlobal.count(id->name)) {
                        localVariables[id->name] = newVarInfo;
                        varInfo = &localVariables[id->name];
                    } else {
                        globalVariables[id->name] = newVarInfo;
                        varInfo = &globalVariables[id->name];
                    }
                }
                
                // Store the value
                assembly << "    mov %rax, -" << varInfo->stackOffset << "(%rbp)  # store " << id->name << "\n";
            } else {
                throw std::runtime_error("Error: Left side of tuple assignment must be variables");
            }
        }
        
        assembly << "    # Tuple assignment complete\n";
    }

    void visit(ChainAssignment& node) override {
        assembly << "    # Chain assignment\n";
        
        // Check if any variables are const
        for (const std::string& varName : node.variables) {
            if (constantVariables.count(varName)) {
                throw std::runtime_error("Error: You are trying to change the value of a constant variable '" + varName + "'");
            }
        }
        
        // Evaluate the value expression once
        node.value->accept(*this);
        // Value is now in %rax - no need to push/pop, just assign directly to each variable
        
        // Determine variable type from the value expression
        std::string varType = "unknown";
        if (auto intLit = dynamic_cast<IntLiteral*>(node.value.get())) {
            varType = "int";
        } else if (auto strLit = dynamic_cast<StringLiteral*>(node.value.get())) {
            varType = "string";
        } else if (auto boolLit = dynamic_cast<BoolLiteral*>(node.value.get())) {
            varType = "bool";
        } else if (auto floatLit = dynamic_cast<FloatLiteral*>(node.value.get())) {
            varType = "float";
        } else if (auto listLit = dynamic_cast<ListLiteral*>(node.value.get())) {
            varType = "list";
        } else if (auto id = dynamic_cast<Identifier*>(node.value.get())) {
            // Variable assignment: copy type from source variable
            auto varInfo = lookupVariable(id->name);
            if (varInfo != nullptr) {
                varType = varInfo->type;
            }
        } else if (auto binExpr = dynamic_cast<BinaryExpression*>(node.value.get())) {
            // Binary expression: infer type from operands
            switch (binExpr->op) {
                case BinaryOp::ADD:
                case BinaryOp::SUB:
                case BinaryOp::MUL:
                case BinaryOp::DIV:
                case BinaryOp::MOD:
                case BinaryOp::FLOOR_DIV:
                case BinaryOp::POWER:
                    // Check if either operand is a float
                    if (isFloatExpression(binExpr->left.get()) || isFloatExpression(binExpr->right.get())) {
                        varType = "float";
                    } else {
                        varType = "int";
                    }
                    break;
                case BinaryOp::EQ:
                case BinaryOp::NE:
                case BinaryOp::LT:
                case BinaryOp::LE:
                case BinaryOp::GT:
                case BinaryOp::GE:
                case BinaryOp::AND:
                case BinaryOp::OR:
                    varType = "bool";  // Comparison/logical operations result in bool
                    break;
                default:
                    varType = "int";  // Default to int for unknown operations
                    break;
            }
        }
        
        // Assign the same value to ALL variables in the chain
        for (const std::string& varName : node.variables) {
            // Find or create variable - use setVariable for consistency
            setVariable(varName, "%rax");
            
            // Update type information for existing variable
            VariableInfo* varInfo = lookupVariable(varName);
            if (varInfo) {
                varInfo->type = varType;
            }
        }
        
        // Value remains in %rax for potential nested assignments or return
    }

    void visit(IndexAssignment& node) override {
        assembly << "    # Index assignment: list[index] = value\n";
        
        // Evaluate the list expression
        node.object->accept(*this);
        assembly << "    mov %rax, %r12  # Save list pointer in %r12\n";
        
        // Evaluate the index expression
        node.index->accept(*this);
        assembly << "    mov %rax, %r13  # Save index in %r13\n";
        
        // Evaluate the value expression  
        node.value->accept(*this);
        assembly << "    mov %rax, %rdx  # Value in %rdx (third argument)\n";
        
        // Call list_set(list, index, value)
        assembly << "    mov %r12, %rdi  # List pointer as first argument\n";
        assembly << "    mov %r13, %rsi  # Index as second argument\n";
        assembly << "    # Value already in %rdx as third argument\n";
        assembly << "    call list_set  # Set list[index] = value\n";
    }

    // Stub implementations for other visitors
    void visit(FloatLiteral& node) override { 
        assembly << "    # Float: " << node.value << "\n"; 
        // Store float value as IEEE 754 in memory and load to rax
        int floatIndex = addFloatLiteral(node.value);
        assembly << "    movq float_" << floatIndex << "(%rip), %rax\n";
    }
    void visit(BoolLiteral& node) override { 
        assembly << "    mov $" << (node.value ? "str_true" : "str_false") << ", %rax\n";
    }
    void visit(UnaryExpression& node) override {
        switch (node.op) {
            case UnaryOp::NOT:
                // Logical NOT: flip boolean result
                node.operand->accept(*this);
                // Check if operand result is falsy (0 or str_false)
                assembly << "    cmp $0, %rax\n";
                assembly << "    je not_true_" << labelCounter << "\n";
                assembly << "    cmp $str_false, %rax\n";
                assembly << "    je not_true_" << labelCounter << "\n";
                // Operand is truthy, so NOT result is false
                assembly << "    mov $str_false, %rax\n";
                assembly << "    jmp not_done_" << labelCounter << "\n";
                assembly << "not_true_" << labelCounter << ":\n";
                // Operand is falsy, so NOT result is true
                assembly << "    mov $str_true, %rax\n";
                assembly << "not_done_" << labelCounter << ":\n";
                labelCounter++;
                break;
            case UnaryOp::PLUS:
                // Unary plus - just evaluate operand
                node.operand->accept(*this);
                break;
            case UnaryOp::MINUS:
                // Unary minus - negate the operand
                node.operand->accept(*this);
                assembly << "    neg %rax\n";
                break;
        }
    }
    void visit(BlockStatement& node) override { 
        for (auto& stmt : node.statements) {
            stmt->accept(*this);
        }
    }
    void visit(ReturnStatement& node) override { 
        if (node.value) node.value->accept(*this);
    }
    void visit(IfStatement& node) override {
        std::string elseLabel = "else_" + std::to_string(labelCounter);
        std::string endLabel = "end_if_" + std::to_string(labelCounter);
        labelCounter++;
        
        // Evaluate condition
        node.condition->accept(*this);
        assembly << "    test %rax, %rax\n";
        assembly << "    jz " << elseLabel << "\n";
        
        // Then branch
        node.thenBranch->accept(*this);
        assembly << "    jmp " << endLabel << "\n";
        
        // Else branch
        assembly << elseLabel << ":\n";
        if (node.elseBranch) {
            node.elseBranch->accept(*this);
        }
        
        assembly << endLabel << ":\n";
    }
    void visit(WhileStatement& node) override {
        std::string loopLabel = "loop_" + std::to_string(labelCounter);
        std::string endLabel = "end_loop_" + std::to_string(labelCounter);
        labelCounter++;
        
        // Store current loop labels for break/continue
        breakLabels.push(endLabel);
        continueLabels.push(loopLabel);
        
        // Loop start
        assembly << loopLabel << ":\n";
        
        // Evaluate condition
        node.condition->accept(*this);
        assembly << "    test %rax, %rax\n";
        assembly << "    jz " << endLabel << "\n";
        
        // Loop body
        node.body->accept(*this);
        
        // Jump back to condition check
        assembly << "    jmp " << loopLabel << "\n";
        
        // Loop end
        assembly << endLabel << ":\n";
        
        // Restore previous loop labels
        breakLabels.pop();
        continueLabels.pop();
    }
    
    // ForStatement removed - only ForInStatement is supported
    
    void visit(ForInStatement& node) override {
        std::string loopLabel = "forin_loop_" + std::to_string(labelCounter);
        std::string endLabel = "forin_end_" + std::to_string(labelCounter);
        std::string isRangeLabel = "forin_is_range_" + std::to_string(labelCounter);
        std::string isListLabel = "forin_is_list_" + std::to_string(labelCounter);
        labelCounter++;
        
        // Store current loop labels for break/continue
        breakLabels.push(endLabel);
        continueLabels.push(loopLabel);
        
        // Evaluate iterable (can be a list or range)
        node.iterable->accept(*this);
        assembly << "    mov %rax, %r12  # Store iterable pointer\n";
        assembly << "    mov $0, %r13    # Initialize index\n";
        
        // Check if iterable is a range or list by checking if it's a result of range() call
        // We'll use a simple heuristic: check if the iterable is a FunctionCall with name "range"
        if (auto funcCall = dynamic_cast<FunctionCall*>(node.iterable.get())) {
            if (funcCall->name == "range") {
                // This is a range object
                assembly << "    # For-in loop over range object\n";
                
                // Get range length
                assembly << "    mov %r12, %rdi  # Range pointer\n";
                assembly << "    call range_len  # Get range length\n";
                assembly << "    mov %rax, %r14  # Store range length\n";
                
                // Loop start
                assembly << loopLabel << ":\n";
                
                // Check if index < length
                assembly << "    cmp %r14, %r13\n";
                assembly << "    jge " << endLabel << "\n";
                
                // Get current element: range[index]
                assembly << "    mov %r12, %rdi  # Range pointer\n";
                assembly << "    mov %r13, %rsi  # Index\n";
                assembly << "    call range_get   # Get element at index\n";
                
                // Store current element in loop variable (range elements are integers)
                setVariable(node.variable, "%rax", "int");
                
                // Execute loop body
                node.body->accept(*this);
                
                // Increment index
                assembly << "    inc %r13\n";
                
                // Jump back to loop condition
                assembly << "    jmp " << loopLabel << "\n";
                
                // Loop end
                assembly << endLabel << ":\n";
            } else {
                // Default to list behavior for other function calls
                assembly << "    # For-in loop over list object\n";
                
                // Get list length
                assembly << "    mov (%r12), %r14  # Load list length\n";
                
                // Loop start
                assembly << loopLabel << ":\n";
                
                // Check if index < length
                assembly << "    cmp %r14, %r13\n";
                assembly << "    jge " << endLabel << "\n";
                
                // Get current element: list[index]
                assembly << "    mov %r12, %rdi  # List pointer\n";
                assembly << "    mov %r13, %rsi  # Index\n";
                assembly << "    call list_get   # Get element at index\n";
                
                // Store current element in loop variable (list elements can be any type, default to int)
                setVariable(node.variable, "%rax", "int");
                
                // Execute loop body
                node.body->accept(*this);
                
                // Increment index
                assembly << "    inc %r13\n";
                
                // Jump back to loop condition
                assembly << "    jmp " << loopLabel << "\n";
                
                // Loop end
                assembly << endLabel << ":\n";
            }
        } else {
            // Default to list behavior for non-function call iterables
            assembly << "    # For-in loop over list object (default)\n";
            
            // Get list length
            assembly << "    mov (%r12), %r14  # Load list length\n";
            
            // Loop start
            assembly << loopLabel << ":\n";
            
            // Check if index < length
            assembly << "    cmp %r14, %r13\n";
            assembly << "    jge " << endLabel << "\n";
            
            // Get current element: list[index]
            assembly << "    mov %r12, %rdi  # List pointer\n";
            assembly << "    mov %r13, %rsi  # Index\n";
            assembly << "    call list_get   # Get element at index\n";
            
            // Store current element in loop variable (list elements can be any type, default to int)
            setVariable(node.variable, "%rax", "int");
            
            // Execute loop body
            node.body->accept(*this);
            
            // Increment index
            assembly << "    inc %r13\n";
            
            // Jump back to loop condition
            assembly << "    jmp " << loopLabel << "\n";
            
            // Loop end
            assembly << endLabel << ":\n";
        }
        
        // Restore previous loop labels
        breakLabels.pop();
        continueLabels.pop();
    }
    
    void visit(BreakStatement& node) override {
        if (breakLabels.empty()) {
            throw std::runtime_error("Break statement not inside a loop");
        }
        assembly << "    jmp " << breakLabels.top() << "\n";
    }
    
    void visit(ContinueStatement& node) override {
        if (continueLabels.empty()) {
            throw std::runtime_error("Continue statement not inside a loop");
        }
        assembly << "    jmp " << continueLabels.top() << "\n";
    }
    
    void visit(PassStatement& node) override {
        // Pass statement does nothing - just emit a comment
        assembly << "    # pass statement\n";
    }
    void visit(GlobalStatement& node) override {
        for (const std::string& varName : node.variables) {
            declaredGlobal.insert(varName);
            assembly << "    # Global declaration: " << varName << "\n";
        }
    }
    
    void visit(LocalStatement& node) override {
        for (const std::string& varName : node.variables) {
            declaredLocal.insert(varName);
            assembly << "    # Local declaration: " << varName << "\n";
        }
    }
    
    void visit(ListLiteral& node) override {
        assembly << "    # Enhanced list literal with " << node.elements.size() << " elements\n";
        
        if (node.elements.empty()) {
            // Create empty list using runtime
            assembly << "    mov $4, %rdi  # Initial capacity for empty list\n";
            assembly << "    call list_new  # Create new empty list\n";
            return;
        }
        
        // For non-empty lists, collect elements in temporary array first
        assembly << "    # Allocating temporary array for " << node.elements.size() << " elements\n";
        size_t tempArraySize = node.elements.size() * 8;  // 8 bytes per element
        assembly << "    mov $" << tempArraySize << ", %rdi\n";
        assembly << "    call orion_malloc  # Allocate temporary array\n";
        assembly << "    mov %rax, %r12  # Save temp array pointer in %r12\n";
        
        // Store each element in temporary array
        for (size_t i = 0; i < node.elements.size(); i++) {
            assembly << "    # Evaluating element " << i << "\n";
            assembly << "    push %r12  # Save temp array pointer\n";
            node.elements[i]->accept(*this);  // Element value in %rax
            assembly << "    pop %r12  # Restore temp array pointer\n";
            assembly << "    movq %rax, " << (i * 8) << "(%r12)  # Store in temp array\n";
        }
        
        // Create list from temporary data
        assembly << "    mov %r12, %rdi  # Temp array pointer\n";
        assembly << "    mov $" << node.elements.size() << ", %rsi  # Element count\n";
        assembly << "    call list_from_data  # Create list from data\n";
        
        // Free temporary array - list_from_data made a copy
        assembly << "    push %rax  # Save list pointer\n";
        assembly << "    mov %r12, %rdi  # Temp array pointer\n";
        assembly << "    call orion_free  # Free temporary array\n";
        assembly << "    pop %rax  # Restore list pointer\n";
    }
    
    void visit(IndexExpression& node) override {
        assembly << "    # Enhanced index expression with negative indexing support\n";
        
        // Evaluate the object (list) - result in %rax
        node.object->accept(*this);
        assembly << "    mov %rax, %rdi  # List pointer as first argument\n";
        
        // Evaluate the index - result in %rax
        node.index->accept(*this);
        assembly << "    mov %rax, %rsi  # Index as second argument\n";
        
        // Call runtime function for safe indexing with negative support
        assembly << "    call list_get  # Get element with bounds checking\n";
        // Result is in %rax - no additional handling needed
    }
    
    void visit(StructDeclaration& node) override { }
    void visit(EnumDeclaration& node) override { }
};


} // namespace orion

// Compiler main function
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <source-file>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    
    try {
        // Read source file
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << filename << std::endl;
            return 1;
        }
        
        std::string source((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        file.close();
        
        // Split source into lines for error reporting
        std::vector<std::string> sourceLines;
        std::stringstream ss(source);
        std::string line;
        while (std::getline(ss, line)) {
            sourceLines.push_back(line);
        }
        
        // Step 1: Lexical analysis
        orion::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        
        // Step 2: Parsing
        orion::SimpleOrionParser parser(tokens);
        auto ast = parser.parse();
        
        // Note: Type checking would be done here for better error messages
        // but we'll focus on runtime error improvements for now
        
        // Step 3: Code generation
        orion::SimpleCodeGenerator codegen;
        std::string assembly = codegen.generate(*ast);
        
        // Step 4: Write assembly to file (KEEP FOR PROOF)
        std::string asmFile = "orion_asm.s";
        std::ofstream asmOut(asmFile);
        asmOut << assembly;
        asmOut.close();
        
        // Step 5: Use GCC to assemble and link with runtime (KEEP EXECUTABLE FOR PROOF)
        std::string exeFile = "orion_exec";
        std::string gccCommand = "gcc -o " + exeFile + " " + asmFile + " runtime.o -lm";
        
        int result = system(gccCommand.c_str());
        if (result != 0) {
            std::cerr << "Error: Failed to assemble program" << std::endl;
            return 1;
        }
        
        // Step 6: Execute the compiled program
        result = system(("./" + exeFile).c_str());
        
        // DON'T clean up - leave files for proof
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}