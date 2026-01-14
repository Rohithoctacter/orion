#include "ast.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>

namespace orion {

class CodeGenerator : public ASTVisitor {
private:
    std::ostringstream output;
    std::unordered_map<std::string, int> labelCounter;
    int nextLabel = 0;
    std::string currentFunction;
    
    // Parameter and variable tracking
    struct VariableInfo {
        int stackOffset;
        bool isParameter;
    };
    std::unordered_map<std::string, VariableInfo> currentVariables;
    int currentStackOffset = 0;
    
public:
    std::string generate(Program& program) {
        output.str("");
        output.clear();
        
        // Generate basic assembly header
        output << ".section .data\n";
        output << "format_int: .string \"%d\\n\"\n";
        output << "format_str: .string \"%s\\n\"\n";
        output << "format_float: .string \"%.2f\\n\"\n";
        output << "\n";
        
        output << ".section .text\n";
        output << ".global _start\n";
        output << "\n";
        
        // Generate code for all statements
        program.accept(*this);
        
        // Add runtime support functions
        generateRuntimeSupport();
        
        return output.str();
    }
    
private:
    std::string newLabel(const std::string& prefix = "L") {
        return prefix + std::to_string(nextLabel++);
    }
    
    void generateRuntimeSupport() {
        output << "\n# Runtime support functions\n";
        
        // Print function (simplified)
        output << "print:\n";
        output << "    push %rbp\n";
        output << "    mov %rsp, %rbp\n";
        output << "    mov %rdi, %rsi\n";
        output << "    mov $format_str, %rdi\n";
        output << "    xor %rax, %rax\n";
        output << "    call printf\n";
        output << "    pop %rbp\n";
        output << "    ret\n";
        output << "\n";
        
        // Print integer
        output << "print_int:\n";
        output << "    push %rbp\n";
        output << "    mov %rsp, %rbp\n";
        output << "    mov %rdi, %rsi\n";
        output << "    mov $format_int, %rdi\n";
        output << "    xor %rax, %rax\n";
        output << "    call printf\n";
        output << "    pop %rbp\n";
        output << "    ret\n";
        output << "\n";
        
        // Exit function
        output << "exit:\n";
        output << "    mov $60, %rax\n";  // sys_exit
        output << "    mov $0, %rdi\n";   // exit status
        output << "    syscall\n";
        output << "\n";
    }
    
public:
    void visit(IntLiteral& node) override {
        output << "    mov $" << node.value << ", %rax\n";
    }
    
    void visit(FloatLiteral& node) override {
        // Simplified float handling
        output << "    # Float literal: " << node.value << "\n";
        output << "    movq $" << static_cast<long long>(node.value) << ", %rax\n";
    }
    
    void visit(StringLiteral& node) override {
        static int stringCounter = 0;
        std::string label = "str_" + std::to_string(stringCounter++);
        
        // Add string to data section (we'll need to track this)
        output << "    # String literal: \"" << node.value << "\"\n";
        output << "    mov $" << label << ", %rax\n";
        
        // Note: In a real implementation, we'd need to add this to the data section
    }
    
    void visit(BoolLiteral& node) override {
        output << "    mov $" << (node.value ? 1 : 0) << ", %rax\n";
    }
    
    void visit(Identifier& node) override {
        // Load variable value from correct stack location
        output << "    # Load variable: " << node.name << "\n";
        
        auto it = currentVariables.find(node.name);
        if (it != currentVariables.end()) {
            // Use the correct stack offset for this variable
            output << "    mov -" << it->second.stackOffset << "(%rbp), %rax  # Load " << node.name;
            if (it->second.isParameter) {
                output << " (parameter)";
            }
            output << "\n";
        } else {
            output << "    # Warning: Unknown variable " << node.name << ", using default location\n";
            output << "    mov -8(%rbp), %rax  # Fallback variable access\n";
        }
    }
    
    void visit(BinaryExpression& node) override {
        // Generate code for left operand
        node.left->accept(*this);
        output << "    push %rax\n";  // Save left operand
        
        // Generate code for right operand
        node.right->accept(*this);
        output << "    mov %rax, %rbx\n";  // Right operand in rbx
        output << "    pop %rax\n";        // Left operand in rax
        
        switch (node.op) {
            case BinaryOp::ADD:
                output << "    add %rbx, %rax\n";
                break;
            case BinaryOp::SUB:
                output << "    sub %rbx, %rax\n";
                break;
            case BinaryOp::MUL:
                output << "    imul %rbx, %rax\n";
                break;
            case BinaryOp::DIV:
                output << "    xor %rdx, %rdx\n";
                output << "    idiv %rbx\n";
                break;
            case BinaryOp::MOD:
                output << "    xor %rdx, %rdx\n";
                output << "    idiv %rbx\n";
                output << "    mov %rdx, %rax\n";
                break;
            case BinaryOp::EQ:
                output << "    cmp %rbx, %rax\n";
                output << "    sete %al\n";
                output << "    movzx %al, %rax\n";
                break;
            case BinaryOp::NE:
                output << "    cmp %rbx, %rax\n";
                output << "    setne %al\n";
                output << "    movzx %al, %rax\n";
                break;
            case BinaryOp::LT:
                output << "    cmp %rbx, %rax\n";
                output << "    setl %al\n";
                output << "    movzx %al, %rax\n";
                break;
            case BinaryOp::LE:
                output << "    cmp %rbx, %rax\n";
                output << "    setle %al\n";
                output << "    movzx %al, %rax\n";
                break;
            case BinaryOp::GT:
                output << "    cmp %rbx, %rax\n";
                output << "    setg %al\n";
                output << "    movzx %al, %rax\n";
                break;
            case BinaryOp::GE:
                output << "    cmp %rbx, %rax\n";
                output << "    setge %al\n";
                output << "    movzx %al, %rax\n";
                break;
            case BinaryOp::AND:
                output << "    and %rbx, %rax\n";
                break;
            case BinaryOp::OR:
                output << "    or %rbx, %rax\n";
                break;
            case BinaryOp::POWER:
                // Simple integer exponentiation using a loop
                output << "    # Power operation: rax = rax ** rbx\n";
                output << "    push %rcx\n";
                output << "    push %rdx\n";
                output << "    mov %rax, %rdx\n";  // base in rdx
                output << "    mov %rbx, %rcx\n";  // exponent in rcx  
                output << "    mov $1, %rax\n";    // result starts at 1
                output << "    test %rcx, %rcx\n"; // check if exponent is 0
                output << "    jz .power_done\n";
                output << ".power_loop:\n";
                output << "    imul %rdx, %rax\n"; // result *= base
                output << "    dec %rcx\n";
                output << "    jnz .power_loop\n";
                output << ".power_done:\n";
                output << "    pop %rdx\n";
                output << "    pop %rcx\n";
                break;
            case BinaryOp::FLOOR_DIV:
                // Floor division - same as regular division for integers
                output << "    xor %rdx, %rdx\n";
                output << "    idiv %rbx\n";
                break;
            default:
                output << "    # Unsupported binary operation\n";
                break;
        }
    }
    
    void visit(UnaryExpression& node) override {
        node.operand->accept(*this);
        
        switch (node.op) {
            case UnaryOp::MINUS:
                output << "    neg %rax\n";
                break;
            case UnaryOp::NOT:
                output << "    test %rax, %rax\n";
                output << "    setz %al\n";
                output << "    movzx %al, %rax\n";
                break;
            case UnaryOp::PLUS:
                // No operation needed
                break;
        }
    }
    
    void visit(FunctionCall& node) override {
        // Simplified function call handling
        if (node.name == "print") {
            if (!node.arguments.empty()) {
                node.arguments[0]->accept(*this);
                output << "    mov %rax, %rdi\n";
                output << "    call print\n";
            }
        } else if (node.name == "str" || node.name == "int") {
            // Type conversion functions - simplified
            if (!node.arguments.empty()) {
                node.arguments[0]->accept(*this);
            }
        } else {
            // Regular function call
            output << "    # Function call: " << node.name << "\n";
            
            // Push arguments (simplified - assumes up to 6 integer arguments)
            for (size_t i = 0; i < node.arguments.size() && i < 6; i++) {
                node.arguments[i]->accept(*this);
                
                // Use calling convention registers
                switch (i) {
                    case 0: output << "    mov %rax, %rdi\n"; break;
                    case 1: output << "    mov %rax, %rsi\n"; break;
                    case 2: output << "    mov %rax, %rdx\n"; break;
                    case 3: output << "    mov %rax, %rcx\n"; break;
                    case 4: output << "    mov %rax, %r8\n"; break;
                    case 5: output << "    mov %rax, %r9\n"; break;
                }
            }
            
            output << "    call " << node.name << "\n";
        }
    }
    
    void visit(VariableDeclaration& node) override {
        output << "    # Variable declaration: " << node.name << "\n";
        
        // Allocate stack space for this variable
        currentStackOffset += 8;
        VariableInfo varInfo;
        varInfo.stackOffset = currentStackOffset;
        varInfo.isParameter = false;
        currentVariables[node.name] = varInfo;
        
        if (node.initializer) {
            node.initializer->accept(*this);
            output << "    mov %rax, -" << currentStackOffset << "(%rbp)  # Store " << node.name << "\n";
        }
    }
    
    void visit(FunctionDeclaration& node) override {
        output << "\n" << node.name << ":\n";
        output << "    push %rbp\n";
        output << "    mov %rsp, %rbp\n";
        
        currentFunction = node.name;
        
        // Clear variables for new function scope
        currentVariables.clear();
        currentStackOffset = 0;
        
        // Set up parameters - move from calling convention registers to stack
        const std::string callingRegs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
        
        output << "    # Setting up function parameters\n";
        for (size_t i = 0; i < node.parameters.size() && i < 6; i++) {
            currentStackOffset += 8;
            VariableInfo paramInfo;
            paramInfo.stackOffset = currentStackOffset;
            paramInfo.isParameter = true;
            currentVariables[node.parameters[i].name] = paramInfo;
            
            // Move parameter from register to stack
            output << "    mov " << callingRegs[i] << ", -" << currentStackOffset 
                   << "(%rbp)  # Parameter " << node.parameters[i].name << "\n";
        }
        
        if (node.isSingleExpression) {
            // Single expression function
            node.expression->accept(*this);
        } else {
            // Block function
            for (auto& stmt : node.body) {
                stmt->accept(*this);
            }
        }
        
        // Function epilogue
        if (node.name == "main") {
            output << "    mov %rax, %rdi\n";  // Return value to exit code
            output << "    call exit\n";
        } else {
            output << "    pop %rbp\n";
            output << "    ret\n";
        }
        
        currentFunction.clear();
    }
    
    void visit(BlockStatement& node) override {
        for (auto& stmt : node.statements) {
            stmt->accept(*this);
        }
    }
    
    void visit(ExpressionStatement& node) override {
        node.expression->accept(*this);
    }
    
    void visit(ReturnStatement& node) override {
        if (node.value) {
            node.value->accept(*this);
        } else {
            output << "    mov $0, %rax\n";  // Default return value
        }
        
        if (currentFunction == "main") {
            output << "    mov %rax, %rdi\n";
            output << "    call exit\n";
        } else {
            output << "    pop %rbp\n";
            output << "    ret\n";
        }
    }
    
    void visit(IfStatement& node) override {
        std::string elseLabel = newLabel("else");
        std::string endLabel = newLabel("end_if");
        
        // Evaluate condition
        node.condition->accept(*this);
        output << "    test %rax, %rax\n";
        output << "    jz " << elseLabel << "\n";
        
        // Then branch
        node.thenBranch->accept(*this);
        output << "    jmp " << endLabel << "\n";
        
        // Else branch
        output << elseLabel << ":\n";
        if (node.elseBranch) {
            node.elseBranch->accept(*this);
        }
        
        output << endLabel << ":\n";
    }
    
    void visit(WhileStatement& node) override {
        std::string loopLabel = newLabel("loop");
        std::string endLabel = newLabel("end_loop");
        
        output << loopLabel << ":\n";
        
        // Evaluate condition
        node.condition->accept(*this);
        output << "    test %rax, %rax\n";
        output << "    jz " << endLabel << "\n";
        
        // Loop body
        node.body->accept(*this);
        output << "    jmp " << loopLabel << "\n";
        
        output << endLabel << ":\n";
    }
    
    // ForStatement removed - only ForInStatement is supported
    
    void visit(StructDeclaration& node) override {
        output << "    # Struct declaration: " << node.name << "\n";
        // Struct layout would be handled by the type system
    }
    
    void visit(EnumDeclaration& node) override {
        output << "    # Enum declaration: " << node.name << "\n";
        // Enum values would be handled as constants
    }
    
    void visit(Program& node) override {
        // Generate _start entry point
        output << "_start:\n";
        
        // Look for main function
        bool hasMain = false;
        for (auto& stmt : node.statements) {
            if (auto func = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                if (func->name == "main") {
                    hasMain = true;
                    break;
                }
            }
        }
        
        // Generate code for top-level statements only (main() must be explicitly called)
        for (auto& stmt : node.statements) {
            if (auto func = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                // Skip function declarations - they're handled separately
                continue;
            }
            stmt->accept(*this);
        }
        
        output << "    call exit\n";
        output << "\n";
        
        // Generate all function definitions
        for (auto& stmt : node.statements) {
            stmt->accept(*this);
        }
    }
};

} // namespace orion
