#include "ast.h"

namespace orion {

// Implement visitor accept methods for all AST nodes

void IntLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void FloatLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void StringLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void InterpolatedString::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void BoolLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void Identifier::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void BinaryExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string BinaryExpression::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "BinaryExpression:\n";
    result += left->toString(indent + 2);
    result += indentStr + "  " + (op == BinaryOp::ADD ? "+" : 
                                  op == BinaryOp::SUB ? "-" :
                                  op == BinaryOp::MUL ? "*" :
                                  op == BinaryOp::DIV ? "/" :
                                  op == BinaryOp::MOD ? "%" :
                                  op == BinaryOp::EQ ? "==" :
                                  op == BinaryOp::NE ? "!=" :
                                  op == BinaryOp::LT ? "<" :
                                  op == BinaryOp::LE ? "<=" :
                                  op == BinaryOp::GT ? ">" :
                                  op == BinaryOp::GE ? ">=" :
                                  op == BinaryOp::AND ? "&&" :
                                  op == BinaryOp::OR ? "||" : "?") + "\n";
    result += right->toString(indent + 2);
    return result;
}

void UnaryExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string UnaryExpression::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "UnaryExpression:\n";
    result += indentStr + "  " + (op == UnaryOp::PLUS ? "+" :
                                  op == UnaryOp::MINUS ? "-" :
                                  op == UnaryOp::NOT ? "!" : "?") + "\n";
    result += operand->toString(indent + 2);
    return result;
}

void FunctionCall::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void TupleExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void ListLiteral::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void IndexExpression::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string FunctionCall::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "FunctionCall(" + name + "):\n";
    for (const auto& arg : arguments) {
        result += arg->toString(indent + 2);
    }
    return result;
}

std::string TupleExpression::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "TupleExpression:\n";
    for (const auto& element : elements) {
        result += element->toString(indent + 2);
    }
    return result;
}

void VariableDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string VariableDeclaration::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "VariableDeclaration(" + name + " : " + type.toString() + "):\n";
    if (initializer) {
        result += initializer->toString(indent + 2);
    }
    return result;
}

void FunctionDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string FunctionDeclaration::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "FunctionDeclaration(" + name + " -> " + returnType.toString() + "):\n";
    
    if (isSingleExpression) {
        result += indentStr + "  Expression:\n";
        if (expression) {
            result += expression->toString(indent + 4);
        }
    } else {
        result += indentStr + "  Body:\n";
        for (const auto& stmt : body) {
            result += stmt->toString(indent + 4);
        }
    }
    return result;
}

void BlockStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string BlockStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "BlockStatement:\n";
    for (const auto& stmt : statements) {
        result += stmt->toString(indent + 2);
    }
    return result;
}

void ExpressionStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void TupleAssignment::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string TupleAssignment::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "TupleAssignment:\n";
    result += indentStr + "  Targets:\n";
    for (const auto& target : targets) {
        result += target->toString(indent + 4);
    }
    result += indentStr + "  Values:\n";
    for (const auto& value : values) {
        result += value->toString(indent + 4);
    }
    return result;
}

void ChainAssignment::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string ChainAssignment::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "ChainAssignment:\n";
    result += indentStr + "  Variables: ";
    for (size_t i = 0; i < variables.size(); i++) {
        if (i > 0) result += ", ";
        result += variables[i];
    }
    result += "\n";
    result += indentStr + "  Value:\n";
    result += value->toString(indent + 4);
    return result;
}

void IndexAssignment::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

void GlobalStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string GlobalStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "GlobalStatement: ";
    for (size_t i = 0; i < variables.size(); i++) {
        if (i > 0) result += ", ";
        result += variables[i];
    }
    result += "\n";
    return result;
}

void LocalStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string LocalStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "LocalStatement: ";
    for (size_t i = 0; i < variables.size(); i++) {
        if (i > 0) result += ", ";
        result += variables[i];
    }
    result += "\n";
    return result;
}

void ReturnStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string ReturnStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "ReturnStatement:\n";
    if (value) {
        result += value->toString(indent + 2);
    }
    return result;
}

void IfStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string IfStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "IfStatement:\n";
    result += indentStr + "  Condition:\n";
    result += condition->toString(indent + 4);
    result += indentStr + "  Then:\n";
    result += thenBranch->toString(indent + 4);
    if (elseBranch) {
        result += indentStr + "  Else:\n";
        result += elseBranch->toString(indent + 4);
    }
    return result;
}

void WhileStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string WhileStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "WhileStatement:\n";
    result += indentStr + "  Condition:\n";
    result += condition->toString(indent + 4);
    result += indentStr + "  Body:\n";
    result += body->toString(indent + 4);
    return result;
}

// ForStatement removed - only ForInStatement is supported

void StructDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string StructDeclaration::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "StructDeclaration(" + name + "):\n";
    for (const auto& field : fields) {
        result += indentStr + "  " + field.name + " : " + field.type.toString() + "\n";
    }
    return result;
}

void EnumDeclaration::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string EnumDeclaration::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "EnumDeclaration(" + name + "):\n";
    for (const auto& value : values) {
        result += indentStr + "  " + value.name + " = " + std::to_string(value.value) + "\n";
    }
    return result;
}

void Program::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string Program::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "Program:\n";
    for (const auto& stmt : statements) {
        result += stmt->toString(indent + 2);
    }
    return result;
}

void ForInStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string ForInStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    std::string result = indentStr + "ForInStatement:\n";
    result += indentStr + "  Variable: " + variable + "\n";
    result += indentStr + "  Iterable:\n";
    result += iterable->toString(indent + 4);
    result += indentStr + "  Body:\n";
    result += body->toString(indent + 4);
    return result;
}

void BreakStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string BreakStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    return indentStr + "BreakStatement";
}

void ContinueStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string ContinueStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    return indentStr + "ContinueStatement";
}

void PassStatement::accept(ASTVisitor& visitor) {
    visitor.visit(*this);
}

std::string PassStatement::toString(int indent) const {
    std::string indentStr(indent, ' ');
    return indentStr + "PassStatement";
}

} // namespace orion