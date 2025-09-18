#include "ast.h"
#include "lexer.h"
#include <iostream>
#include <memory>
#include <vector>
#include <string>

namespace orion {

class Parser {
private:
    std::vector<Token> tokens;
    size_t current;
    
public:
    Parser(const std::vector<Token>& toks) : tokens(toks), current(0) {}
    
    std::unique_ptr<Program> parse() {
        auto program = std::make_unique<Program>();
        
        while (!isAtEnd()) {
            // Skip newlines at top level
            if (peek().type == TokenType::NEWLINE) {
                advance();
                continue;
            }
            
            auto stmt = parseStatement();
            if (stmt) {
                program->statements.push_back(std::move(stmt));
            }
        }
        
        return program;
    }
    
private:
    bool isAtEnd() const {
        return current >= tokens.size() || peek().type == TokenType::EOF_TOKEN;
    }
    
    const Token& peek() const {
        if (current >= tokens.size()) {
            static Token eof(TokenType::EOF_TOKEN, "", 0, 0);
            return eof;
        }
        return tokens[current];
    }
    
    const Token& previous() const {
        return tokens[current - 1];
    }
    
    Token advance() {
        if (!isAtEnd()) current++;
        return previous();
    }
    
    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }
    
    bool match(std::initializer_list<TokenType> types) {
        for (auto type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }
        return false;
    }
    
    bool isStatementTerminator() const {
        return check(TokenType::NEWLINE) || check(TokenType::SEMICOLON) || 
               check(TokenType::RBRACE) || check(TokenType::EOF_TOKEN) ||
               check(TokenType::IF) || check(TokenType::ELIF) || check(TokenType::ELSE) ||
               check(TokenType::WHILE) || check(TokenType::FOR) || 
               check(TokenType::BREAK) || check(TokenType::CONTINUE) || 
               check(TokenType::PASS) || check(TokenType::RETURN);
    }
    
    Token consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();
        
        throw std::runtime_error("Parse error at line " + std::to_string(peek().line) + 
                                ": " + message + ". Got " + peek().typeToString());
    }
    
    std::unique_ptr<Statement> parseStatement() {
        try {
            // Function declarations (only when using 'fn' keyword)
            if (check(TokenType::IDENTIFIER) && peek().value == "fn") {
                advance(); // consume 'fn'
                return parseFunctionDeclaration();
            }
            
            // Check for tuple assignment
            if (check(TokenType::LPAREN)) {
                return parseTupleAssignmentOrExpression();
            }
            
            // Other statements
            if (match({TokenType::GLOBAL})) return parseGlobalStatement();
            if (match({TokenType::LOCAL})) return parseLocalStatement();
            if (match({TokenType::STRUCT})) return parseStructDeclaration();
            if (match({TokenType::ENUM})) return parseEnumDeclaration();
            if (match({TokenType::IF})) return parseIfStatement();
            if (match({TokenType::WHILE})) return parseWhileStatement();
            if (match({TokenType::FOR})) return parseForStatement();
            if (match({TokenType::RETURN})) return parseReturnStatement();
            if (match({TokenType::BREAK})) return parseBreakStatement();
            if (match({TokenType::CONTINUE})) return parseContinueStatement();
            if (match({TokenType::PASS})) return parsePassStatement();
            if (match({TokenType::LBRACE})) return parseBlockStatement();
            
            // Variable declaration or expression statement
            return parseVariableDeclarationOrExpression();
        } catch (const std::exception& e) {
            // Skip to next statement on error
            synchronize();
            throw;
        }
    }
    
    std::unique_ptr<FunctionDeclaration> parseFunctionDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expected function name");
        auto func = std::make_unique<FunctionDeclaration>(name.value, Type(TypeKind::VOID));
        
        consume(TokenType::LPAREN, "Expected '(' after function name");
        
        // Parse parameters
        if (!check(TokenType::RPAREN)) {
            do {
                Token paramName = consume(TokenType::IDENTIFIER, "Expected parameter name");
                
                Type paramType;
                bool hasExplicitType = true;
                
                // Check for colon-based type annotation: name: Type
                if (check(TokenType::COLON)) {
                    advance(); // consume the colon
                    paramType = parseType();
                }
                // Check for direct type (existing syntax): name Type  
                else if (isTypeToken(peek())) {
                    paramType = parseType();
                }
                // No type specified - implicit typing: name
                else {
                    paramType = Type(TypeKind::UNKNOWN);
                    hasExplicitType = false;
                }
                
                func->parameters.emplace_back(paramName.value, paramType, hasExplicitType);
            } while (match({TokenType::COMMA}));
        }
        
        consume(TokenType::RPAREN, "Expected ')' after parameters");
        
        // Parse return type
        if (match({TokenType::ARROW})) {
            func->returnType = parseType();
        }
        
        // Parse body - single expression or block
        if (match({TokenType::FAT_ARROW})) {
            // Single expression function
            func->isSingleExpression = true;
            func->expression = parseExpression();
        } else {
            // Block function
            consume(TokenType::LBRACE, "Expected '{' or '=>' for function body");
            current--; // Back up to parse block
            auto block = parseBlockStatement();
            func->body = std::move(static_cast<BlockStatement*>(block.get())->statements);
            block.release(); // We moved the contents
        }
        
        return func;
    }
    
    std::unique_ptr<Statement> parseVariableDeclarationOrExpression() {
        // Try to parse as variable declaration first
        size_t savedPos = current;
        
        try {
            return parseVariableDeclaration();
        } catch (...) {
            // Not a variable declaration, try expression statement
            current = savedPos;
            auto expr = parseExpression();
            
            // Skip optional newline/semicolon
            match({TokenType::NEWLINE, TokenType::SEMICOLON});
            
            return std::make_unique<ExpressionStatement>(std::move(expr));
        }
    }
    
    std::unique_ptr<VariableDeclaration> parseVariableDeclaration() {
        // Support multiple syntax forms:
        // a = 5
        // int a = 5
        // a int = 5
        // a = int 5
        
        Token first = advance();
        
        if (first.type == TokenType::IDENTIFIER) {
            std::string varName = first.value;
            
            if (match({TokenType::ASSIGN, TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN, 
                      TokenType::MULTIPLY_ASSIGN, TokenType::DIVIDE_ASSIGN, TokenType::MODULO_ASSIGN})) {
                TokenType assignOp = tokens[current - 1].type; // Get the operator token
                
                if (assignOp == TokenType::ASSIGN) {
                    // a = expr or a = type expr
                    if (isTypeKeyword(peek().type)) {
                        // a = type expr
                        Type type = parseType();
                        auto init = parseExpression();
                        return std::make_unique<VariableDeclaration>(varName, type, std::move(init), true);
                    } else {
                        // a = expr (type inference)
                        auto init = parseExpression();
                        return std::make_unique<VariableDeclaration>(varName, Type(), std::move(init), false);
                    }
                } else {
                    // Compound assignment: a += expr becomes a = a + expr
                    BinaryOp binaryOp;
                    switch (assignOp) {
                        case TokenType::PLUS_ASSIGN: binaryOp = BinaryOp::ADD; break;
                        case TokenType::MINUS_ASSIGN: binaryOp = BinaryOp::SUB; break;
                        case TokenType::MULTIPLY_ASSIGN: binaryOp = BinaryOp::MUL; break;
                        case TokenType::DIVIDE_ASSIGN: binaryOp = BinaryOp::DIV; break;
                        case TokenType::MODULO_ASSIGN: binaryOp = BinaryOp::MOD; break;
                        default: throw std::runtime_error("Invalid compound assignment operator");
                    }
                    
                    // Parse the right-hand side
                    auto rightExpr = parseExpression();
                    
                    // Create desugared assignment: x op= y becomes x = x op y
                    auto leftId = std::make_unique<Identifier>(varName);
                    auto binaryExpr = std::make_unique<BinaryExpression>(std::move(leftId), binaryOp, std::move(rightExpr));
                    
                    return std::make_unique<VariableDeclaration>(varName, Type(), std::move(binaryExpr), false);
                }
            } else if (isTypeKeyword(peek().type)) {
                // a int = expr
                Type type = parseType();
                consume(TokenType::ASSIGN, "Expected '=' after type in variable declaration");
                auto init = parseExpression();
                return std::make_unique<VariableDeclaration>(varName, type, std::move(init), true);
            }
        } else if (isTypeKeyword(first.type)) {
            // type a = expr
            Type type = tokenToType(first.type, first.value);
            Token varName = consume(TokenType::IDENTIFIER, "Expected variable name after type");
            consume(TokenType::ASSIGN, "Expected '=' in variable declaration");
            auto init = parseExpression();
            return std::make_unique<VariableDeclaration>(varName.value, type, std::move(init), true);
        }
        
        throw std::runtime_error("Invalid variable declaration syntax");
    }
    
    std::unique_ptr<Statement> parseTupleAssignmentOrExpression() {
        // Parse what looks like a tuple
        auto tupleExpr = parseExpression();
        
        // Check if it's followed by assignment (including compound assignment)
        if (match({TokenType::ASSIGN, TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN, 
                  TokenType::MULTIPLY_ASSIGN, TokenType::DIVIDE_ASSIGN, TokenType::MODULO_ASSIGN})) {
            TokenType assignOp = tokens[current - 1].type; // Get the operator token
            
            if (assignOp == TokenType::ASSIGN) {
            // This is a tuple assignment
            auto assignment = std::make_unique<TupleAssignment>();
            
            // Extract targets from the tuple expression
            if (auto tuple = dynamic_cast<TupleExpression*>(tupleExpr.get())) {
                // Move elements from tuple to assignment targets
                for (auto& element : tuple->elements) {
                    assignment->targets.push_back(std::move(element));
                }
                tupleExpr.release(); // We've moved the contents
            } else {
                // Single target (not actually a tuple)
                assignment->targets.push_back(std::move(tupleExpr));
            }
            
            // Parse right side - could be a tuple or single expression
            auto rightExpr = parseExpression();
            if (auto rightTuple = dynamic_cast<TupleExpression*>(rightExpr.get())) {
                // Move elements from right tuple to assignment values
                for (auto& element : rightTuple->elements) {
                    assignment->values.push_back(std::move(element));
                }
                rightExpr.release(); // We've moved the contents
            } else {
                // Single value
                assignment->values.push_back(std::move(rightExpr));
            }
            
            match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
            return std::move(assignment);
            } else {
                // Compound assignment on tuple/expression: not supported for now
                throw std::runtime_error("Compound assignment is only supported for simple variables");
            }
        } else {
            // Not an assignment, just a regular expression statement
            match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
            return std::make_unique<ExpressionStatement>(std::move(tupleExpr));
        }
    }
    
    std::unique_ptr<Statement> parseStructDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expected struct name");
        auto structDecl = std::make_unique<StructDeclaration>(name.value);
        
        consume(TokenType::LBRACE, "Expected '{' after struct name");
        
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (match({TokenType::NEWLINE})) continue;
            
            Token fieldName = consume(TokenType::IDENTIFIER, "Expected field name");
            Type fieldType = parseType();
            
            structDecl->fields.emplace_back(fieldName.value, fieldType);
            match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional separator
        }
        
        consume(TokenType::RBRACE, "Expected '}' after struct fields");
        return std::move(structDecl);
    }
    
    std::unique_ptr<Statement> parseEnumDeclaration() {
        Token name = consume(TokenType::IDENTIFIER, "Expected enum name");
        auto enumDecl = std::make_unique<EnumDeclaration>(name.value);
        
        consume(TokenType::LBRACE, "Expected '{' after enum name");
        
        int value = 0;
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (match({TokenType::NEWLINE})) continue;
            
            Token valueName = consume(TokenType::IDENTIFIER, "Expected enum value name");
            
            // Check for explicit value assignment
            if (match({TokenType::ASSIGN})) {
                Token valueToken = consume(TokenType::INTEGER, "Expected integer value");
                value = std::stoi(valueToken.value);
            }
            
            enumDecl->values.emplace_back(valueName.value, value++);
            
            if (!check(TokenType::RBRACE)) {
                match({TokenType::COMMA, TokenType::NEWLINE}); // Optional separator
            }
        }
        
        consume(TokenType::RBRACE, "Expected '}' after enum values");
        return std::move(enumDecl);
    }
    
    std::unique_ptr<Statement> parseGlobalStatement() {
        auto globalStmt = std::make_unique<GlobalStatement>();
        
        // Parse comma-separated variable names
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected variable name after 'global'");
        }
        
        do {
            Token varName = consume(TokenType::IDENTIFIER, "Expected identifier in global statement");
            globalStmt->variables.push_back(varName.value);
        } while (match({TokenType::COMMA}));
        
        match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
        return std::move(globalStmt);
    }
    
    std::unique_ptr<Statement> parseLocalStatement() {
        auto localStmt = std::make_unique<LocalStatement>();
        
        // Parse comma-separated variable names  
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected variable name after 'local'");
        }
        
        do {
            Token varName = consume(TokenType::IDENTIFIER, "Expected identifier in local statement");
            localStmt->variables.push_back(varName.value);
        } while (match({TokenType::COMMA}));
        
        match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
        return std::move(localStmt);
    }
    
    std::unique_ptr<Statement> parseIfStatement() {
        auto condition = parseExpression();
        auto thenBranch = parseStatement();
        
        auto ifStmt = std::make_unique<IfStatement>(std::move(condition), std::move(thenBranch));
        
        if (match({TokenType::ELIF})) {
            // Handle elif as nested if-else
            current--; // Back up
            ifStmt->elseBranch = parseStatement(); // This will parse the elif as another if
        } else if (match({TokenType::ELSE})) {
            ifStmt->elseBranch = parseStatement();
        }
        
        return std::move(ifStmt);
    }
    
    std::unique_ptr<Statement> parseWhileStatement() {
        auto condition = parseExpression();
        auto body = parseStatement();
        
        return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
    }
    
    std::unique_ptr<Statement> parseForStatement() {
        // Only support Python-style for-in loops: for variable in iterable { body }
        Token variable = consume(TokenType::IDENTIFIER, "Expected variable name after 'for' in for-in loop");
        
        if (!check(TokenType::IN)) {
            throw std::runtime_error("Expected 'in' after variable in for-in loop. C-style for loops are not supported.");
        }
        advance(); // consume 'in'
        
        auto iterable = parseExpression();
        auto body = parseStatement();
        
        return std::make_unique<ForInStatement>(variable.value, std::move(iterable), std::move(body));
    }
    
    std::unique_ptr<Statement> parseReturnStatement() {
        std::unique_ptr<Expression> value = nullptr;
        
        if (!check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) && !isAtEnd()) {
            value = parseExpression();
        }
        
        match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
        return std::make_unique<ReturnStatement>(std::move(value));
    }
    
    std::unique_ptr<Statement> parseBreakStatement() {
        match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
        return std::make_unique<BreakStatement>();
    }
    
    std::unique_ptr<Statement> parseContinueStatement() {
        match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
        return std::make_unique<ContinueStatement>();
    }
    
    std::unique_ptr<Statement> parsePassStatement() {
        match({TokenType::NEWLINE, TokenType::SEMICOLON}); // Optional terminator
        return std::make_unique<PassStatement>();
    }
    
    std::unique_ptr<Statement> parseBlockStatement() {
        auto block = std::make_unique<BlockStatement>();
        
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (match({TokenType::NEWLINE})) continue;
            
            auto stmt = parseStatement();
            if (stmt) {
                block->statements.push_back(std::move(stmt));
            }
        }
        
        consume(TokenType::RBRACE, "Expected '}' after block");
        return std::move(block);
    }
    
    std::unique_ptr<Expression> parseExpression() {
        return parseLogicalOr();
    }
    
    // Parse expression but stop at statement terminators
    std::unique_ptr<Expression> parseExpressionWithTerminators() {
        auto expr = parseLogicalOr();
        
        // If we hit a statement terminator, return the expression as-is
        if (isStatementTerminator()) {
            return expr;
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseLogicalOr() {
        auto expr = parseLogicalAnd();
        
        while (!isStatementTerminator() && match({TokenType::OR})) {
            BinaryOp op = BinaryOp::OR;
            auto right = parseLogicalAnd();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseLogicalAnd() {
        auto expr = parseEquality();
        
        while (!isStatementTerminator() && match({TokenType::AND})) {
            BinaryOp op = BinaryOp::AND;
            auto right = parseEquality();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseEquality() {
        auto expr = parseComparison();
        
        while (!isStatementTerminator() && match({TokenType::EQ, TokenType::NE})) {
            BinaryOp op = (previous().type == TokenType::EQ) ? BinaryOp::EQ : BinaryOp::NE;
            auto right = parseComparison();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseComparison() {
        auto expr = parseTerm();
        
        while (!isStatementTerminator() && match({TokenType::LT, TokenType::LE, TokenType::GT, TokenType::GE})) {
            BinaryOp op;
            switch (previous().type) {
                case TokenType::LT: op = BinaryOp::LT; break;
                case TokenType::LE: op = BinaryOp::LE; break;
                case TokenType::GT: op = BinaryOp::GT; break;
                case TokenType::GE: op = BinaryOp::GE; break;
                default: throw std::runtime_error("Invalid comparison operator");
            }
            auto right = parseTerm();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseTerm() {
        auto expr = parseFactor();
        
        while (!isStatementTerminator() && match({TokenType::PLUS, TokenType::MINUS})) {
            BinaryOp op = (previous().type == TokenType::PLUS) ? BinaryOp::ADD : BinaryOp::SUB;
            auto right = parseFactor();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseFactor() {
        auto expr = parsePower();
        
        while (!isStatementTerminator() && match({TokenType::MULTIPLY, TokenType::DIVIDE, TokenType::MODULO, TokenType::FLOOR_DIVIDE})) {
            BinaryOp op;
            switch (previous().type) {
                case TokenType::MULTIPLY: op = BinaryOp::MUL; break;
                case TokenType::DIVIDE: op = BinaryOp::DIV; break;
                case TokenType::MODULO: op = BinaryOp::MOD; break;
                case TokenType::FLOOR_DIVIDE: op = BinaryOp::FLOOR_DIV; break;
                default: throw std::runtime_error("Invalid factor operator");
            }
            auto right = parsePower();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parsePower() {
        auto expr = parseUnary();
        
        // Exponentiation is right-associative, so we handle it differently
        if (match({TokenType::POWER})) {
            auto right = parsePower(); // Right-associative: a**b**c = a**(b**c)
            expr = std::make_unique<BinaryExpression>(std::move(expr), BinaryOp::POWER, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseUnary() {
        if (match({TokenType::NOT, TokenType::MINUS, TokenType::PLUS})) {
            UnaryOp op;
            switch (previous().type) {
                case TokenType::NOT: op = UnaryOp::NOT; break;
                case TokenType::MINUS: op = UnaryOp::MINUS; break;
                case TokenType::PLUS: op = UnaryOp::PLUS; break;
                default: throw std::runtime_error("Invalid unary operator");
            }
            auto right = parseUnary();
            return std::make_unique<UnaryExpression>(op, std::move(right));
        }
        
        return parseCall();
    }
    
    std::unique_ptr<Expression> parseCall() {
        auto expr = parsePrimary();
        
        while (true) {
            if (match({TokenType::LPAREN})) {
                // Function call
                if (auto id = dynamic_cast<Identifier*>(expr.get())) {
                    auto call = std::make_unique<FunctionCall>(id->name);
                    expr.release(); // Release ownership since we're replacing it
                    
                    // Parse arguments
                    if (!check(TokenType::RPAREN)) {
                        do {
                            call->arguments.push_back(parseExpression());
                        } while (match({TokenType::COMMA}));
                    }
                    
                    consume(TokenType::RPAREN, "Expected ')' after arguments");
                    expr = std::move(call);
                } else {
                    throw std::runtime_error("Invalid function call");
                }
            } else {
                break;
            }
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parsePrimary() {
        if (match({TokenType::TRUE, TokenType::FALSE})) {
            return std::make_unique<BoolLiteral>(previous().value == "True");
        }
        
        if (match({TokenType::INTEGER})) {
            return std::make_unique<IntLiteral>(std::stoi(previous().value));
        }
        
        if (match({TokenType::FLOAT})) {
            return std::make_unique<FloatLiteral>(std::stod(previous().value));
        }
        
        if (match({TokenType::STRING})) {
            return std::make_unique<StringLiteral>(previous().value);
        }
        
        if (match({TokenType::IDENTIFIER})) {
            return std::make_unique<Identifier>(previous().value);
        }
        
        if (match({TokenType::LPAREN})) {
            // Check if this is a tuple or just a parenthesized expression
            auto firstExpr = parseExpression();
            
            if (match({TokenType::COMMA})) {
                // This is a tuple
                auto tuple = std::make_unique<TupleExpression>();
                tuple->elements.push_back(std::move(firstExpr));
                
                do {
                    tuple->elements.push_back(parseExpression());
                } while (match({TokenType::COMMA}));
                
                consume(TokenType::RPAREN, "Expected ')' after tuple");
                return std::move(tuple);
            } else {
                // Just a parenthesized expression
                consume(TokenType::RPAREN, "Expected ')' after expression");
                return firstExpr;
            }
        }
        
        throw std::runtime_error("Unexpected token in expression: " + peek().value);
    }
    
    Type parseType() {
        if (match({TokenType::INT})) return Type(TypeKind::INT32);
        if (match({TokenType::INT64})) return Type(TypeKind::INT64);
        if (match({TokenType::FLOAT32})) return Type(TypeKind::FLOAT32);
        if (match({TokenType::FLOAT64})) return Type(TypeKind::FLOAT64);
        if (match({TokenType::STRING_TYPE})) return Type(TypeKind::STRING);
        if (match({TokenType::BOOL_TYPE})) return Type(TypeKind::BOOL);
        if (match({TokenType::VOID})) return Type(TypeKind::VOID);
        
        if (check(TokenType::IDENTIFIER)) {
            Token name = advance();
            return Type(TypeKind::STRUCT, name.value); // Could be struct or enum
        }
        
        throw std::runtime_error("Expected type");
    }
    
    bool isTypeKeyword(TokenType type) const {
        return type == TokenType::INT || type == TokenType::INT64 ||
               type == TokenType::FLOAT32 || type == TokenType::FLOAT64 ||
               type == TokenType::STRING_TYPE || type == TokenType::BOOL_TYPE ||
               type == TokenType::VOID;
    }
    
    bool isTypeToken(const Token& token) const {
        return isTypeKeyword(token.type) || token.type == TokenType::IDENTIFIER;
    }
    
    Type tokenToType(TokenType type, const std::string& value) {
        switch (type) {
            case TokenType::INT: return Type(TypeKind::INT32);
            case TokenType::INT64: return Type(TypeKind::INT64);
            case TokenType::FLOAT32: return Type(TypeKind::FLOAT32);
            case TokenType::FLOAT64: return Type(TypeKind::FLOAT64);
            case TokenType::STRING_TYPE: return Type(TypeKind::STRING);
            case TokenType::BOOL_TYPE: return Type(TypeKind::BOOL);
            case TokenType::VOID: return Type(TypeKind::VOID);
            default: return Type(TypeKind::UNKNOWN, value);
        }
    }
    
    void synchronize() {
        advance();
        
        while (!isAtEnd()) {
            if (previous().type == TokenType::SEMICOLON || previous().type == TokenType::NEWLINE) {
                return;
            }
            
            switch (peek().type) {
                case TokenType::STRUCT:
                case TokenType::ENUM:
                case TokenType::IF:
                case TokenType::WHILE:
                case TokenType::FOR:
                case TokenType::RETURN:
                    return;
                default:
                    break;
            }
            
            advance();
        }
    }
};

} // namespace orion
