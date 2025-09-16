#pragma once
#include <string>
#include <vector>

namespace orion {

enum class TargetPlatform {
    LINUX_X86_64,
    MACOS_X86_64,
    WINDOWS_X86_64
};

// Abstract interface for platform-specific code generation
class TargetBackend {
public:
    virtual ~TargetBackend() = default;
    
    // Platform-specific assembly directives
    virtual std::string getDataSection() const = 0;
    virtual std::string getTextSection() const = 0;
    virtual std::string getGlobalDirective(const std::string& symbol) const = 0;
    virtual std::string getExternDirective(const std::string& symbol) const = 0;
    
    // Platform-specific symbol naming
    virtual std::string getPlatformSymbol(const std::string& symbol) const = 0;
    
    // Calling convention specifics
    virtual std::vector<std::string> getArgumentRegisters() const = 0;
    virtual std::string getReturnRegister() const = 0;
    virtual int getStackAlignment() const = 0;
    virtual std::string getStackReservation(int bytes) const = 0;
    
    // Platform identification
    virtual TargetPlatform getPlatform() const = 0;
    virtual std::string getPlatformName() const = 0;
    
    // File extensions
    virtual std::string getAssemblyExtension() const = 0;
    virtual std::string getExecutableExtension() const = 0;
    
    // Assembler command generation
    virtual std::string getAssemblerCommand(const std::string& asmFile, 
                                          const std::string& objFile,
                                          const std::string& exeFile) const = 0;
    
    // Platform-specific memory operand formatting (Intel vs AT&T)
    virtual std::string getMemoryOperand(const std::string& base, int offset) const = 0;
    
    // Platform-specific data directives
    virtual std::string getStringDirective(const std::string& label, const std::string& value) const = 0;
    virtual std::string getQuadDirective(const std::string& label, uint64_t value) const = 0;
    
protected:
    TargetBackend() = default;
};

// Linux x86-64 backend using GNU Assembler and ELF format
class LinuxX86_64Backend : public TargetBackend {
public:
    std::string getDataSection() const override {
        return ".section .data\n";
    }
    
    std::string getTextSection() const override {
        return "\n.section .text\n";
    }
    
    std::string getGlobalDirective(const std::string& symbol) const override {
        return ".global " + symbol + "\n";
    }
    
    std::string getExternDirective(const std::string& symbol) const override {
        return ".extern " + symbol + "\n";
    }
    
    std::string getPlatformSymbol(const std::string& symbol) const override {
        return symbol; // No prefix needed on Linux
    }
    
    std::vector<std::string> getArgumentRegisters() const override {
        return {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    }
    
    std::string getReturnRegister() const override {
        return "%rax";
    }
    
    int getStackAlignment() const override {
        return 16; // 16-byte alignment required
    }
    
    std::string getStackReservation(int bytes) const override {
        // Align to 16-byte boundary
        int aligned = ((bytes + 15) / 16) * 16;
        return "    sub $" + std::to_string(aligned) + ", %rsp\n";
    }
    
    TargetPlatform getPlatform() const override {
        return TargetPlatform::LINUX_X86_64;
    }
    
    std::string getPlatformName() const override {
        return "linux-x86_64";
    }
    
    std::string getAssemblyExtension() const override {
        return ".s";
    }
    
    std::string getExecutableExtension() const override {
        return "";
    }
    
    std::string getAssemblerCommand(const std::string& asmFile, 
                                   const std::string& objFile,
                                   const std::string& exeFile) const override {
        return "gcc -o " + exeFile + " " + asmFile + " runtime.o -lm";
    }
    
    // Linux AT&T syntax memory operand
    std::string getMemoryOperand(const std::string& base, int offset) const override {
        if (offset == 0) {
            return "(%" + base + ")";
        } else {
            return std::to_string(offset) + "(%" + base + ")";
        }
    }
    
    // Linux GAS data directives
    std::string getStringDirective(const std::string& label, const std::string& value) const override {
        return label + ": .string \"" + value + "\"\n";
    }
    
    std::string getQuadDirective(const std::string& label, uint64_t value) const override {
        return label + ": .quad " + std::to_string(value) + "\n";
    }
};

// macOS x86-64 backend using GNU Assembler with Mach-O format
class MacOSX86_64Backend : public TargetBackend {
public:
    std::string getDataSection() const override {
        return ".section __DATA,__data\n";
    }
    
    std::string getTextSection() const override {
        return "\n.section __TEXT,__text\n";
    }
    
    std::string getGlobalDirective(const std::string& symbol) const override {
        return ".globl " + symbol + "\n";  // Don't double-prefix - caller handles platform symbol
    }
    
    std::string getExternDirective(const std::string& symbol) const override {
        return ".extern " + symbol + "\n";  // Don't double-prefix - caller handles platform symbol
    }
    
    std::string getPlatformSymbol(const std::string& symbol) const override {
        return "_" + symbol; // macOS requires underscore prefix
    }
    
    std::vector<std::string> getArgumentRegisters() const override {
        return {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    }
    
    std::string getReturnRegister() const override {
        return "%rax";
    }
    
    int getStackAlignment() const override {
        return 16; // 16-byte alignment required
    }
    
    std::string getStackReservation(int bytes) const override {
        // Align to 16-byte boundary
        int aligned = ((bytes + 15) / 16) * 16;
        return "    sub $" + std::to_string(aligned) + ", %rsp\n";
    }
    
    TargetPlatform getPlatform() const override {
        return TargetPlatform::MACOS_X86_64;
    }
    
    std::string getPlatformName() const override {
        return "macos-x86_64";
    }
    
    std::string getAssemblyExtension() const override {
        return ".s";
    }
    
    std::string getExecutableExtension() const override {
        return "";
    }
    
    std::string getAssemblerCommand(const std::string& asmFile, 
                                   const std::string& objFile,
                                   const std::string& exeFile) const override {
        return "gcc -o " + exeFile + " " + asmFile + " runtime.o -lm";
    }
    
    // macOS AT&T syntax memory operand (same as Linux)
    std::string getMemoryOperand(const std::string& base, int offset) const override {
        if (offset == 0) {
            return "(%" + base + ")";
        } else {
            return std::to_string(offset) + "(%" + base + ")";
        }
    }
    
    // macOS GAS data directives (same as Linux)
    std::string getStringDirective(const std::string& label, const std::string& value) const override {
        return label + ": .string \"" + value + "\"\n";
    }
    
    std::string getQuadDirective(const std::string& label, uint64_t value) const override {
        return label + ": .quad " + std::to_string(value) + "\n";
    }
};

// Windows x86-64 backend using AT&T syntax and Win64 calling convention
class WindowsX86_64Backend : public TargetBackend {
public:
    std::string getDataSection() const override {
        return ".section .data\n";  // GAS syntax for data section
    }
    
    std::string getTextSection() const override {
        return "\n.section .text\n";  // AT&T syntax for Windows (no Intel directive)
    }
    
    // Windows-specific data directives (GAS syntax)
    std::string getStringDirective(const std::string& label, const std::string& value) const override {
        return label + ": .string \"" + value + "\"\n";  // GAS string directive with null terminator
    }
    
    std::string getQuadDirective(const std::string& label, uint64_t value) const override {
        return label + ": .quad " + std::to_string(value) + "\n";  // GAS 64-bit quad word
    }
    
    std::string getIntDirective(const std::string& label, const std::string& value) const {
        return label + ": db '" + value + "', 0\n";  // Format string directive
    }
    
    std::string getGlobalDirective(const std::string& symbol) const override {
        return ".global " + symbol + "\n";  // GAS syntax - caller handles platform symbol
    }
    
    std::string getExternDirective(const std::string& symbol) const override {
        return ".extern " + symbol + "\n";  // GAS syntax - caller handles platform symbol  
    }
    
    std::string getPlatformSymbol(const std::string& symbol) const override {
        return symbol; // No prefix needed on Windows with MSVC CRT
    }
    
    std::vector<std::string> getArgumentRegisters() const override {
        // Windows x64 calling convention: first 4 args in registers (AT&T syntax)
        return {"%rcx", "%rdx", "%r8", "%r9"};
    }
    
    std::string getReturnRegister() const override {
        return "%rax";
    }
    
    int getStackAlignment() const override {
        return 16; // 16-byte alignment required
    }
    
    std::string getStackReservation(int bytes) const override {
        // Windows requires 32 bytes of shadow space + local variables
        int shadowSpace = 32;
        int aligned = ((bytes + shadowSpace + 15) / 16) * 16;
        return "    subq $" + std::to_string(aligned) + ", %rsp\n";
    }
    
    // Windows AT&T syntax memory operand formatting
    std::string getMemoryOperand(const std::string& base, int offset) const override {
        if (offset == 0) {
            return "(%" + base + ")";
        } else {
            return std::to_string(offset) + "(%" + base + ")";
        }
    }
    
    // Windows calling convention - shadow space management (AT&T syntax)
    std::string getShadowSpaceSetup() const {
        return "    subq $32, %rsp  # Allocate shadow space\n";
    }
    
    std::string getShadowSpaceCleanup() const {
        return "    addq $32, %rsp  # Clean up shadow space\n";
    }
    
    TargetPlatform getPlatform() const override {
        return TargetPlatform::WINDOWS_X86_64;
    }
    
    std::string getPlatformName() const override {
        return "windows-x86_64";
    }
    
    std::string getAssemblyExtension() const override {
        return ".s";  // Use .s for GAS compatibility
    }
    
    std::string getExecutableExtension() const override {
        return ".exe";
    }
    
    std::string getAssemblerCommand(const std::string& asmFile, 
                                   const std::string& objFile,
                                   const std::string& exeFile) const override {
        // Use GCC with AT&T syntax for Windows - compatible with mingw64
        // This allows clean assembly generation that works directly with GCC
        return "gcc -m64 -o " + exeFile + " " + asmFile + " runtime.o -lm";
    }
};

// Factory function to create appropriate backend based on platform
std::unique_ptr<TargetBackend> createTargetBackend(TargetPlatform platform) {
    switch (platform) {
        case TargetPlatform::LINUX_X86_64:
            return std::make_unique<LinuxX86_64Backend>();
        case TargetPlatform::MACOS_X86_64:
            return std::make_unique<MacOSX86_64Backend>();
        case TargetPlatform::WINDOWS_X86_64:
            return std::make_unique<WindowsX86_64Backend>();
        default:
            return std::make_unique<LinuxX86_64Backend>(); // Default fallback
    }
}

// Utility function to detect current platform
TargetPlatform detectCurrentPlatform() {
#ifdef _WIN32
    return TargetPlatform::WINDOWS_X86_64;
#elif __APPLE__
    return TargetPlatform::MACOS_X86_64;
#else
    return TargetPlatform::LINUX_X86_64; // Default to Linux
#endif
}

} // namespace orion