#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace orion {

// Forward declarations
class ASTVisitor;

// Base AST node class
class ASTNode {
public:
    int line = 0;
    int column = 0;
    
    ASTNode(int l = 0, int c = 0) : line(l), column(c) {}
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
    virtual std::string toString(int indent = 0) const = 0;
};

// Expression base class
class Expression : public ASTNode {
public:
    Expression(int line = 0, int column = 0) : ASTNode(line, column) {}
    virtual ~Expression() = default;
};

// Statement base class
class Statement : public ASTNode {
public:
    Statement(int line = 0, int column = 0) : ASTNode(line, column) {}
    virtual ~Statement() = default;
};

// Type representation
enum class TypeKind {
    INT32,
    INT64,
    FLOAT32,
    FLOAT64,
    BOOL,
    STRING,
    VOID,
    STRUCT,
    ENUM,
    FUNCTION,
    LIST,
    DICT,
    UNKNOWN
};

struct Type {
    TypeKind kind;
    std::string name;
    std::unique_ptr<Type> elementType;  // For LIST: element type, For DICT: value type
    std::unique_ptr<Type> keyType;      // For DICT: key type
    
    Type(TypeKind k = TypeKind::UNKNOWN, const std::string& n = "") 
        : kind(k), name(n), elementType(nullptr), keyType(nullptr) {}
    
    Type(TypeKind k, std::unique_ptr<Type> elemType)
        : kind(k), name(""), elementType(std::move(elemType)), keyType(nullptr) {}
    
    Type(TypeKind k, std::unique_ptr<Type> kType, std::unique_ptr<Type> vType)
        : kind(k), name(""), keyType(std::move(kType)), elementType(std::move(vType)) {}
    
    // Copy constructor
    Type(const Type& other) : kind(other.kind), name(other.name) {
        if (other.elementType) {
            elementType = std::make_unique<Type>(*other.elementType);
        }
        if (other.keyType) {
            keyType = std::make_unique<Type>(*other.keyType);
        }
    }
    
    // Assignment operator
    Type& operator=(const Type& other) {
        if (this != &other) {
            kind = other.kind;
            name = other.name;
            if (other.elementType) {
                elementType = std::make_unique<Type>(*other.elementType);
            } else {
                elementType = nullptr;
            }
            if (other.keyType) {
                keyType = std::make_unique<Type>(*other.keyType);
            } else {
                keyType = nullptr;
            }
        }
        return *this;
    }
    
    std::string toString() const {
        switch (kind) {
            case TypeKind::INT32: return "int";
            case TypeKind::INT64: return "int64";
            case TypeKind::FLOAT32: return "float";
            case TypeKind::FLOAT64: return "float64";
            case TypeKind::BOOL: return "bool";
            case TypeKind::STRING: return "string";
            case TypeKind::VOID: return "void";
            case TypeKind::STRUCT: return "struct " + name;
            case TypeKind::ENUM: return "enum " + name;
            case TypeKind::LIST: 
                return "list[" + (elementType ? elementType->toString() : "unknown") + "]";
            case TypeKind::DICT:
                return "dict[" + (keyType ? keyType->toString() : "unknown") + ", " + 
                       (elementType ? elementType->toString() : "unknown") + "]";
            default: return "unknown";
        }
    }
};

// Literals
class IntLiteral : public Expression {
public:
    int32_t value;
    
    IntLiteral(int32_t val, int line = 0, int column = 0) : Expression(line, column), value(val) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "IntLiteral(" + std::to_string(value) + ")";
    }
};

class FloatLiteral : public Expression {
public:
    double value;
    
    FloatLiteral(double val, int line = 0, int column = 0) : Expression(line, column), value(val) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "FloatLiteral(" + std::to_string(value) + ")";
    }
};

class StringLiteral : public Expression {
public:
    std::string value;
    
    StringLiteral(const std::string& val, int line = 0, int column = 0) : Expression(line, column), value(val) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "StringLiteral(\"" + value + "\")";
    }
};

// Interpolated string containing both text parts and expressions
class InterpolatedString : public Expression {
public:
    struct Part {
        bool isExpression;
        std::string text;  // Used when isExpression is false
        std::unique_ptr<Expression> expression;  // Used when isExpression is true
        
        Part(const std::string& t) : isExpression(false), text(t) {}
        Part(std::unique_ptr<Expression> expr) : isExpression(true), expression(std::move(expr)) {}
    };
    
    std::vector<Part> parts;
    
    InterpolatedString(int line = 0, int column = 0) : Expression(line, column) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        std::string result = std::string(indent, ' ') + "InterpolatedString:\n";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (parts[i].isExpression) {
                result += std::string(indent + 2, ' ') + "Expression:\n";
                result += parts[i].expression->toString(indent + 4);
            } else {
                result += std::string(indent + 2, ' ') + "Text(\"" + parts[i].text + "\")\n";
            }
        }
        return result;
    }
};

class BoolLiteral : public Expression {
public:
    bool value;
    
    BoolLiteral(bool val, int line = 0, int column = 0) : Expression(line, column), value(val) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "BoolLiteral(" + (value ? "True" : "False") + ")";
    }
};

// Identifier
class Identifier : public Expression {
public:
    std::string name;
    
    Identifier(const std::string& n, int line = 0, int column = 0) : Expression(line, column), name(n) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "Identifier(" + name + ")";
    }
};

// Binary operations
enum class BinaryOp {
    ADD, SUB, MUL, DIV, MOD,
    POWER, FLOOR_DIV,
    EQ, NE, LT, LE, GT, GE,
    AND, OR,
    ASSIGN
};

class BinaryExpression : public Expression {
public:
    std::unique_ptr<Expression> left;
    BinaryOp op;
    std::unique_ptr<Expression> right;
    
    BinaryExpression(std::unique_ptr<Expression> l, BinaryOp o, std::unique_ptr<Expression> r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Unary operations
enum class UnaryOp {
    PLUS, MINUS, NOT
};

class UnaryExpression : public Expression {
public:
    UnaryOp op;
    std::unique_ptr<Expression> operand;
    
    UnaryExpression(UnaryOp o, std::unique_ptr<Expression> expr)
        : op(o), operand(std::move(expr)) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Function call
class FunctionCall : public Expression {
public:
    std::string name;
    std::vector<std::unique_ptr<Expression>> arguments;
    
    FunctionCall(const std::string& n) : name(n) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Tuple expression
class TupleExpression : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elements;
    
    TupleExpression() {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// List literal: [1, 2, 3, "hello"]
class ListLiteral : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> elements;
    
    ListLiteral(int line = 0, int column = 0) : Expression(line, column) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        std::string result = std::string(indent, ' ') + "ListLiteral([";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) result += ", ";
            result += elements[i]->toString(0);
        }
        result += "])";
        return result;
    }
};

// Dictionary literal: {"name": "John", "age": 30}
class DictLiteral : public Expression {
public:
    std::vector<std::unique_ptr<Expression>> keys;
    std::vector<std::unique_ptr<Expression>> values;
    
    DictLiteral(int line = 0, int column = 0) : Expression(line, column) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        std::string result = std::string(indent, ' ') + "DictLiteral({";
        for (size_t i = 0; i < keys.size(); ++i) {
            if (i > 0) result += ", ";
            result += keys[i]->toString(0) + ": " + values[i]->toString(0);
        }
        result += "})";
        return result;
    }
};

// Index access: list[0]
class IndexExpression : public Expression {
public:
    std::unique_ptr<Expression> object;  // The list being indexed
    std::unique_ptr<Expression> index;   // The index expression
    
    IndexExpression(std::unique_ptr<Expression> obj, std::unique_ptr<Expression> idx, int line = 0, int column = 0)
        : Expression(line, column), object(std::move(obj)), index(std::move(idx)) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "IndexExpression(" + object->toString(0) + "[" + index->toString(0) + "])";
    }
};

// Variable declaration
class VariableDeclaration : public Statement {
public:
    std::string name;
    Type type;
    std::unique_ptr<Expression> initializer;
    bool hasExplicitType;
    bool isConstant;
    
    VariableDeclaration(const std::string& n, const Type& t, std::unique_ptr<Expression> init, bool explicit_type = false, bool constant = false)
        : name(n), type(t), initializer(std::move(init)), hasExplicitType(explicit_type), isConstant(constant) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Function parameter
struct Parameter {
    std::string name;
    Type type;
    bool isExplicitType;  // true if type was explicitly provided, false if inferred
    
    Parameter(const std::string& n, const Type& t, bool explicit_type = true) 
        : name(n), type(t), isExplicitType(explicit_type) {}
};

// Function declaration
class FunctionDeclaration : public Statement {
public:
    std::string name;
    std::vector<Parameter> parameters;
    Type returnType;
    std::vector<std::unique_ptr<Statement>> body;
    bool isSingleExpression;
    std::unique_ptr<Expression> expression; // for single-expression functions
    
    FunctionDeclaration(const std::string& n, const Type& ret)
        : name(n), returnType(ret), isSingleExpression(false) {}
    
    // Helper method to check if function has implicit parameters
    bool hasImplicitParams() const {
        for (const auto& param : parameters) {
            if (!param.isExplicitType) return true;
        }
        return false;
    }
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Block statement
class BlockStatement : public Statement {
public:
    std::vector<std::unique_ptr<Statement>> statements;
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Expression statement
class ExpressionStatement : public Statement {
public:
    std::unique_ptr<Expression> expression;
    
    ExpressionStatement(std::unique_ptr<Expression> expr)
        : expression(std::move(expr)) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "ExpressionStatement:\n" + 
               expression->toString(indent + 2);
    }
};

// Tuple assignment for (a,b) = (c,d) syntax
class TupleAssignment : public Statement {
public:
    std::vector<std::unique_ptr<Expression>> targets;  // left side (a,b)
    std::vector<std::unique_ptr<Expression>> values;   // right side (c,d)
    
    TupleAssignment() {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Chain assignment for a=b=5 syntax
class ChainAssignment : public Statement {
public:
    std::vector<std::string> variables;  // variables to assign to (a, b)
    std::unique_ptr<Expression> value;   // value to assign (5)
    
    ChainAssignment() {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Index assignment: list[0] = value
class IndexAssignment : public Statement {
public:
    std::unique_ptr<Expression> object;  // The list being indexed
    std::unique_ptr<Expression> index;   // The index expression
    std::unique_ptr<Expression> value;   // The value to assign
    
    IndexAssignment(std::unique_ptr<Expression> obj, std::unique_ptr<Expression> idx, std::unique_ptr<Expression> val)
        : object(std::move(obj)), index(std::move(idx)), value(std::move(val)) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override {
        return "IndexAssignment(" + object->toString(0) + "[" + index->toString(0) + "] = " + value->toString(0) + ")";
    }
};

// Global statement
class GlobalStatement : public Statement {
public:
    std::vector<std::string> variables;  // global x, y, z
    
    GlobalStatement() {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Local statement
class LocalStatement : public Statement {
public:
    std::vector<std::string> variables;  // local x, y, z
    
    LocalStatement() {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Return statement
class ReturnStatement : public Statement {
public:
    std::unique_ptr<Expression> value;
    
    ReturnStatement(std::unique_ptr<Expression> val = nullptr)
        : value(std::move(val)) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// If statement
class IfStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> thenBranch;
    std::unique_ptr<Statement> elseBranch;
    
    IfStatement(std::unique_ptr<Expression> cond, std::unique_ptr<Statement> then_stmt)
        : condition(std::move(cond)), thenBranch(std::move(then_stmt)) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// While statement
class WhileStatement : public Statement {
public:
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> body;
    
    WhileStatement(std::unique_ptr<Expression> cond, std::unique_ptr<Statement> body_stmt)
        : condition(std::move(cond)), body(std::move(body_stmt)) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Note: C-style ForStatement removed - only ForInStatement is supported

// Python-style for-in statement
class ForInStatement : public Statement {
public:
    std::string variable;                        // for x in ...
    std::unique_ptr<Expression> iterable;        // ... in iterable
    std::unique_ptr<Statement> body;
    
    ForInStatement(const std::string& var, 
                   std::unique_ptr<Expression> iter,
                   std::unique_ptr<Statement> body_stmt)
        : variable(var), iterable(std::move(iter)), body(std::move(body_stmt)) {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Break statement
class BreakStatement : public Statement {
public:
    BreakStatement() {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Continue statement
class ContinueStatement : public Statement {
public:
    ContinueStatement() {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Pass statement
class PassStatement : public Statement {
public:
    PassStatement() {}
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Struct field
struct StructField {
    std::string name;
    Type type;
    
    StructField(const std::string& n, const Type& t) : name(n), type(t) {}
};

// Struct declaration
class StructDeclaration : public Statement {
public:
    std::string name;
    std::vector<StructField> fields;
    
    StructDeclaration(const std::string& n) : name(n) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Enum value
struct EnumValue {
    std::string name;
    int value;
    
    EnumValue(const std::string& n, int v) : name(n), value(v) {}
};

// Enum declaration
class EnumDeclaration : public Statement {
public:
    std::string name;
    std::vector<EnumValue> values;
    
    EnumDeclaration(const std::string& n) : name(n) {}
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Program (root node)
class Program : public ASTNode {
public:
    std::vector<std::unique_ptr<Statement>> statements;
    
    void accept(ASTVisitor& visitor) override;
    std::string toString(int indent = 0) const override;
};

// Visitor pattern for AST traversal
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    virtual void visit(IntLiteral& node) = 0;
    virtual void visit(FloatLiteral& node) = 0;
    virtual void visit(StringLiteral& node) = 0;
    virtual void visit(InterpolatedString& node) = 0;
    virtual void visit(BoolLiteral& node) = 0;
    virtual void visit(Identifier& node) = 0;
    virtual void visit(BinaryExpression& node) = 0;
    virtual void visit(UnaryExpression& node) = 0;
    virtual void visit(FunctionCall& node) = 0;
    virtual void visit(TupleExpression& node) = 0;
    virtual void visit(ListLiteral& node) = 0;
    virtual void visit(DictLiteral& node) = 0;
    virtual void visit(IndexExpression& node) = 0;
    virtual void visit(VariableDeclaration& node) = 0;
    virtual void visit(FunctionDeclaration& node) = 0;
    virtual void visit(BlockStatement& node) = 0;
    virtual void visit(ExpressionStatement& node) = 0;
    virtual void visit(TupleAssignment& node) = 0;
    virtual void visit(ChainAssignment& node) = 0;
    virtual void visit(IndexAssignment& node) = 0;
    virtual void visit(GlobalStatement& node) = 0;
    virtual void visit(LocalStatement& node) = 0;
    virtual void visit(ReturnStatement& node) = 0;
    virtual void visit(IfStatement& node) = 0;
    virtual void visit(WhileStatement& node) = 0;
    virtual void visit(ForInStatement& node) = 0;
    virtual void visit(BreakStatement& node) = 0;
    virtual void visit(ContinueStatement& node) = 0;
    virtual void visit(PassStatement& node) = 0;
    virtual void visit(StructDeclaration& node) = 0;
    virtual void visit(EnumDeclaration& node) = 0;
    virtual void visit(Program& node) = 0;
};

} // namespace orion

#endif // AST_H
