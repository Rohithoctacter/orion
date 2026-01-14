#ifndef SIMPLE_PARSER_H
#define SIMPLE_PARSER_H

#include "ast.h"
#include "lexer.h"
#include <memory>
#include <vector>
#include <stdexcept>

namespace orion {

class SimpleOrionParser {
private:
    std::vector<Token> tokens;
    size_t current;
    
public:
    SimpleOrionParser(const std::vector<Token>& toks) : tokens(toks), current(0) {}
    
    std::unique_ptr<Program> parse() {
        auto program = std::make_unique<Program>();
        
        while (!isAtEnd()) {
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
    
    Token advance() {
        if (!isAtEnd()) current++;
        return tokens[current - 1];
    }
    
    bool check(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }
    
    bool checkNext(TokenType type) const {
        if (current + 1 >= tokens.size()) return false;
        return tokens[current + 1].type == type;
    }
    
    bool isStatementStarter(TokenType type) const {
        return type == TokenType::WHILE || type == TokenType::FOR || 
               type == TokenType::IF || type == TokenType::RETURN ||
               type == TokenType::BREAK || type == TokenType::CONTINUE ||
               type == TokenType::PASS || type == TokenType::RBRACE ||
               type == TokenType::EOF_TOKEN || type == TokenType::GLOBAL ||
               type == TokenType::LOCAL;
    }
    
    std::unique_ptr<Statement> parseStatement() {
        // Return statement - check first to ensure it's caught
        if (check(TokenType::RETURN) || (check(TokenType::IDENTIFIER) && peek().value == "return")) {
            advance(); // consume 'return'
            
            // Check if there's an expression to return
            std::unique_ptr<Expression> returnValue = nullptr;
            if (!check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) && 
                !check(TokenType::RBRACE) && !isAtEnd()) {
                returnValue = parseExpression();
            }
            
            return std::make_unique<ReturnStatement>(std::move(returnValue));
        }
        
        // Check for tuple assignment
        if (check(TokenType::LPAREN)) {
            return parseTupleAssignmentOrExpression();
        }
        
        // Global statement
        if (check(TokenType::GLOBAL)) {
            return parseGlobalStatement();
        }
        
        // Local statement
        if (check(TokenType::LOCAL)) {
            return parseLocalStatement();
        }
        
        // Function declaration (fn name() { ... })
        if (check(TokenType::IDENTIFIER) && peek().value == "fn") {
            return parseFunctionDeclaration();
        }
        
        // If statement
        if (check(TokenType::IF)) {
            return parseIfStatement();
        }
        
        // While statement
        if (check(TokenType::WHILE)) {
            return parseWhileStatement();
        }
        
        // For statement (both C-style and Python for-in)
        if (check(TokenType::FOR)) {
            return parseForStatement();
        }
        
        // Break statement
        if (check(TokenType::BREAK)) {
            advance(); // consume 'break'
            return std::make_unique<BreakStatement>();
        }
        
        // Continue statement
        if (check(TokenType::CONTINUE)) {
            advance(); // consume 'continue'
            return std::make_unique<ContinueStatement>();
        }
        
        // Pass statement
        if (check(TokenType::PASS)) {
            advance(); // consume 'pass'
            return std::make_unique<PassStatement>();
        }
        
        // Variable declaration or expression
        return parseVariableDeclarationOrExpression();
    }
    
    std::unique_ptr<FunctionDeclaration> parseFunctionDeclaration() {
        advance(); // consume 'fn'
        
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected function name");
        }
        
        std::string funcName = advance().value;
        auto func = std::make_unique<FunctionDeclaration>(funcName, Type(TypeKind::VOID));
        
        if (!check(TokenType::LPAREN)) {
            throw std::runtime_error("Expected '(' after function name");
        }
        advance(); // consume '('
        
        // Parse parameters
        if (!check(TokenType::RPAREN)) {
            do {
                if (!check(TokenType::IDENTIFIER)) {
                    throw std::runtime_error("Expected parameter name");
                }
                
                std::string paramName = advance().value;
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
                
                func->parameters.emplace_back(paramName, paramType, hasExplicitType);
            } while (match(TokenType::COMMA));
        }
        advance(); // consume ')'
        
        if (!check(TokenType::LBRACE)) {
            throw std::runtime_error("Expected '{' for function body");
        }
        advance(); // consume '{'
        
        // Parse function body
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (check(TokenType::NEWLINE)) {
                advance();
                continue;
            }
            
            auto stmt = parseStatement();
            if (stmt) {
                func->body.push_back(std::move(stmt));
            }
        }
        
        if (!check(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after function body");
        }
        advance(); // consume '}'
        
        return func;
    }
    
    std::unique_ptr<GlobalStatement> parseGlobalStatement() {
        advance(); // consume 'global'
        
        auto globalStmt = std::make_unique<GlobalStatement>();
        
        // Parse comma-separated variable names
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected variable name after 'global'");
        }
        
        do {
            if (!check(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected identifier in global statement");
            }
            globalStmt->variables.push_back(advance().value);
        } while (check(TokenType::COMMA) && (advance(), true));
        
        return globalStmt;
    }
    
    std::unique_ptr<LocalStatement> parseLocalStatement() {
        advance(); // consume 'local'
        
        auto localStmt = std::make_unique<LocalStatement>();
        
        // Parse comma-separated variable names
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected variable name after 'local'");
        }
        
        do {
            if (!check(TokenType::IDENTIFIER)) {
                throw std::runtime_error("Expected identifier in local statement");
            }
            localStmt->variables.push_back(advance().value);
        } while (check(TokenType::COMMA) && (advance(), true));
        
        return localStmt;
    }
    
    std::unique_ptr<IfStatement> parseIfStatement() {
        advance(); // consume 'if'
        
        // Parse condition
        auto condition = parseExpression();
        
        // Expect opening brace
        if (!check(TokenType::LBRACE)) {
            throw std::runtime_error("Expected '{' after if condition");
        }
        advance(); // consume '{'
        
        // Parse then branch (statements until '}')
        auto thenBlock = std::make_unique<BlockStatement>();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (check(TokenType::NEWLINE)) {
                advance();
                continue;
            }
            
            auto stmt = parseStatement();
            if (stmt) {
                thenBlock->statements.push_back(std::move(stmt));
            }
        }
        
        if (!check(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after if block");
        }
        advance(); // consume '}'
        
        auto ifStmt = std::make_unique<IfStatement>(std::move(condition), std::move(thenBlock));
        
        // Handle elif/else
        if (check(TokenType::ELIF)) {
            // Parse elif as nested if
            ifStmt->elseBranch = parseIfStatement();
        } else if (check(TokenType::ELSE)) {
            advance(); // consume 'else'
            
            if (!check(TokenType::LBRACE)) {
                throw std::runtime_error("Expected '{' after else");
            }
            advance(); // consume '{'
            
            auto elseBlock = std::make_unique<BlockStatement>();
            while (!check(TokenType::RBRACE) && !isAtEnd()) {
                if (check(TokenType::NEWLINE)) {
                    advance();
                    continue;
                }
                
                auto stmt = parseStatement();
                if (stmt) {
                    elseBlock->statements.push_back(std::move(stmt));
                }
            }
            
            if (!check(TokenType::RBRACE)) {
                throw std::runtime_error("Expected '}' after else block");
            }
            advance(); // consume '}'
            
            ifStmt->elseBranch = std::move(elseBlock);
        }
        
        return ifStmt;
    }
    
    std::unique_ptr<Statement> parseTupleAssignmentOrExpression() {
        // Parse what looks like a tuple
        auto tupleExpr = parseExpression();
        
        // Check if it's followed by assignment
        if (check(TokenType::ASSIGN)) {
            advance(); // consume '='
            
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
            
            return std::move(assignment);
        } else {
            // Not an assignment, just a regular expression statement
            return std::make_unique<ExpressionStatement>(std::move(tupleExpr));
        }
    }

    std::unique_ptr<Statement> parseVariableDeclarationOrExpression() {
        // Check for const keyword first
        bool isConstant = false;
        if (check(TokenType::CONST)) {
            advance(); // consume 'const'
            isConstant = true;
        }
        
        // Check for index assignment first: list[index] = value
        if (check(TokenType::IDENTIFIER)) {
            // Look ahead to see if this is an index assignment
            size_t lookahead = current + 1;
            if (lookahead < tokens.size() && tokens[lookahead].type == TokenType::LBRACKET) {
                // Find the matching closing bracket
                size_t bracketCount = 0;
                size_t closeBracket = lookahead;
                while (closeBracket < tokens.size()) {
                    if (tokens[closeBracket].type == TokenType::LBRACKET) {
                        bracketCount++;
                    } else if (tokens[closeBracket].type == TokenType::RBRACKET) {
                        bracketCount--;
                        if (bracketCount == 0) break;
                    }
                    closeBracket++;
                }
                
                // Check if there's an assignment after the closing bracket
                if (closeBracket + 1 < tokens.size() && tokens[closeBracket + 1].type == TokenType::ASSIGN) {
                    // This is an index assignment: list[index] = value
                    std::string listName = advance().value; // consume identifier
                    advance(); // consume '['
                    
                    auto indexExpr = parseExpression();
                    
                    if (!check(TokenType::RBRACKET)) {
                        throw std::runtime_error("Expected ']' after index expression");
                    }
                    advance(); // consume ']'
                    
                    if (!check(TokenType::ASSIGN)) {
                        throw std::runtime_error("Expected '=' after index expression");
                    }
                    advance(); // consume '='
                    
                    auto valueExpr = parseExpression();
                    auto listExpr = std::make_unique<Identifier>(listName);
                    return std::make_unique<IndexAssignment>(std::move(listExpr), std::move(indexExpr), std::move(valueExpr));
                }
            }
        }
        
        // Check for assignment patterns: chain assignment (a=b=5) and compound assignment (a+=5)
        if (check(TokenType::IDENTIFIER)) {
            // Scan ahead to detect chain assignment pattern
            std::vector<size_t> assignPositions;
            size_t lookahead = current;
            
            while (lookahead < tokens.size()) {
                if (tokens[lookahead].type == TokenType::ASSIGN) {
                    assignPositions.push_back(lookahead);
                } else if (tokens[lookahead].type == TokenType::NEWLINE || 
                          tokens[lookahead].type == TokenType::SEMICOLON ||
                          tokens[lookahead].type == TokenType::EOF_TOKEN) {
                    break;
                }
                lookahead++;
            }
            
            if (assignPositions.size() > 1) {
                // Chain assignment detected (a=b=5)
                auto chainAssign = std::make_unique<ChainAssignment>();
                
                // Parse variables: for "a=b=5", we need [a, b]
                size_t pos = current;
                for (size_t i = 0; i < assignPositions.size(); i++) {
                    size_t assignPos = assignPositions[i];
                    // Get the identifier before this assignment
                    if (pos < assignPos && tokens[pos].type == TokenType::IDENTIFIER) {
                        chainAssign->variables.push_back(tokens[pos].value);
                    }
                    // Move to the position after this = sign for next variable
                    pos = assignPos + 1;
                }
                
                // Move current to the last assignment position + 1 (the value)
                current = assignPositions.back() + 1;
                chainAssign->value = parseExpression();
                
                return std::move(chainAssign);
            } else if (assignPositions.size() == 1) {
                // Simple variable assignment: name = value
                std::string varName = advance().value;
                advance(); // consume '='
                
                auto init = parseExpression();
                return std::make_unique<VariableDeclaration>(varName, Type(), std::move(init), false, isConstant);
            }
            
            // Check for compound assignment operators
            if (check(TokenType::IDENTIFIER)) {
                std::string varName = advance().value;
                
                // Check for compound assignment operators
                if (check(TokenType::PLUS_ASSIGN) || check(TokenType::MINUS_ASSIGN) || 
                    check(TokenType::MULTIPLY_ASSIGN) || check(TokenType::DIVIDE_ASSIGN) || 
                    check(TokenType::MODULO_ASSIGN)) {
                    
                    // Map compound operator to binary operator
                    BinaryOp binaryOp;
                    TokenType compoundToken = peek().type;
                    switch (compoundToken) {
                        case TokenType::PLUS_ASSIGN: binaryOp = BinaryOp::ADD; break;
                        case TokenType::MINUS_ASSIGN: binaryOp = BinaryOp::SUB; break;
                        case TokenType::MULTIPLY_ASSIGN: binaryOp = BinaryOp::MUL; break;
                        case TokenType::DIVIDE_ASSIGN: binaryOp = BinaryOp::DIV; break;
                        case TokenType::MODULO_ASSIGN: binaryOp = BinaryOp::MOD; break;
                        default: throw std::runtime_error("Invalid compound assignment operator");
                    }
                    
                    advance(); // consume compound operator
                    
                    // Parse the right-hand side expression
                    auto rightExpr = parseExpression();
                    
                    // Create desugared assignment: x op= y becomes x = x op y
                    auto leftId = std::make_unique<Identifier>(varName);
                    auto binaryExpr = std::make_unique<BinaryExpression>(std::move(leftId), binaryOp, std::move(rightExpr));
                    
                    // Create the assignment statement
                    return std::make_unique<VariableDeclaration>(varName, Type(), std::move(binaryExpr), false, isConstant);
                }
                
                // If we consumed the identifier but didn't find compound assignment,
                // we need to backtrack and handle as expression
                current--; // backtrack
            }
        }
        
        // Handle case where const is used without assignment
        if (isConstant) {
            throw std::runtime_error("Constant variable must be initialized");
        }
        
        // Expression statement
        auto expr = parseExpression();
        return std::make_unique<ExpressionStatement>(std::move(expr));
    }
    
    std::unique_ptr<Expression> parseExpression() {
        return parseLogicalOr();
    }
    
    std::unique_ptr<Expression> parseLogicalOr() {
        auto expr = parseLogicalAnd();
        
        while (check(TokenType::OR) && !isStatementStarter(peek().type)) {
            advance(); // consume '||'
            auto right = parseLogicalAnd();
            expr = std::make_unique<BinaryExpression>(std::move(expr), BinaryOp::OR, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseLogicalAnd() {
        auto expr = parseEquality();
        
        while (check(TokenType::AND) && !isStatementStarter(peek().type)) {
            advance(); // consume '&&'
            auto right = parseEquality();
            expr = std::make_unique<BinaryExpression>(std::move(expr), BinaryOp::AND, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseEquality() {
        auto expr = parseComparison();
        
        while ((check(TokenType::EQ) || check(TokenType::NE)) && !isStatementStarter(peek().type)) {
            BinaryOp op = (peek().type == TokenType::EQ) ? BinaryOp::EQ : BinaryOp::NE;
            advance();
            auto right = parseComparison();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseComparison() {
        auto expr = parseTerm();
        
        while ((check(TokenType::LT) || check(TokenType::LE) || check(TokenType::GT) || check(TokenType::GE)) && !isStatementStarter(peek().type)) {
            BinaryOp op;
            switch (peek().type) {
                case TokenType::LT: op = BinaryOp::LT; break;
                case TokenType::LE: op = BinaryOp::LE; break;
                case TokenType::GT: op = BinaryOp::GT; break;
                case TokenType::GE: op = BinaryOp::GE; break;
                default: throw std::runtime_error("Invalid comparison operator");
            }
            advance();
            auto right = parseTerm();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseTerm() {
        auto expr = parseFactor();
        
        while ((check(TokenType::PLUS) || check(TokenType::MINUS)) && !isStatementStarter(peek().type)) {
            BinaryOp op = (peek().type == TokenType::PLUS) ? BinaryOp::ADD : BinaryOp::SUB;
            advance();
            auto right = parseFactor();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseFactor() {
        auto expr = parsePower();
        
        while ((check(TokenType::MULTIPLY) || check(TokenType::DIVIDE) || check(TokenType::MODULO) || check(TokenType::FLOOR_DIVIDE)) && !isStatementStarter(peek().type)) {
            BinaryOp op;
            switch (peek().type) {
                case TokenType::MULTIPLY: op = BinaryOp::MUL; break;
                case TokenType::DIVIDE: op = BinaryOp::DIV; break;
                case TokenType::MODULO: op = BinaryOp::MOD; break;
                case TokenType::FLOOR_DIVIDE: op = BinaryOp::FLOOR_DIV; break;
                default: throw std::runtime_error("Invalid factor operator");
            }
            advance();
            auto right = parsePower();
            expr = std::make_unique<BinaryExpression>(std::move(expr), op, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parsePower() {
        auto expr = parseUnary();
        
        // Exponentiation is right-associative
        if (check(TokenType::POWER)) {
            advance(); // consume '**'
            auto right = parsePower(); // Right-associative: a**b**c = a**(b**c)
            expr = std::make_unique<BinaryExpression>(std::move(expr), BinaryOp::POWER, std::move(right));
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parseUnary() {
        if (check(TokenType::NOT) || check(TokenType::MINUS) || check(TokenType::PLUS)) {
            UnaryOp op;
            switch (peek().type) {
                case TokenType::NOT: op = UnaryOp::NOT; break;
                case TokenType::MINUS: op = UnaryOp::MINUS; break;
                case TokenType::PLUS: op = UnaryOp::PLUS; break;
                default: throw std::runtime_error("Invalid unary operator");
            }
            advance();
            auto right = parseUnary();
            return std::make_unique<UnaryExpression>(op, std::move(right));
        }
        
        return parseCall();
    }
    
    std::unique_ptr<Expression> parseCall() {
        auto expr = parsePrimary();
        
        while (true) {
            if (check(TokenType::LPAREN)) {
                // Function call
                if (auto id = dynamic_cast<Identifier*>(expr.get())) {
                    advance(); // consume '('
                    auto call = std::make_unique<FunctionCall>(id->name);
                    expr.release(); // Release ownership since we're replacing it
                    
                    // Parse arguments
                    if (!check(TokenType::RPAREN)) {
                        do {
                            call->arguments.push_back(parseExpression());
                        } while (check(TokenType::COMMA) && (advance(), true));
                    }
                    
                    if (!check(TokenType::RPAREN)) {
                        throw std::runtime_error("Expected ')' after function arguments");
                    }
                    advance(); // consume ')'
                    expr = std::move(call);
                } else {
                    throw std::runtime_error("Invalid function call");
                }
            } else if (check(TokenType::LBRACKET)) {
                // Index access: expr[index]
                advance(); // consume '['
                auto index = parseExpression();
                
                if (!check(TokenType::RBRACKET)) {
                    throw std::runtime_error("Expected ']' after index expression");
                }
                advance(); // consume ']'
                
                // Create IndexExpression and transfer ownership
                auto indexExpr = std::make_unique<IndexExpression>(std::move(expr), std::move(index));
                expr = std::move(indexExpr);
            } else {
                break;
            }
        }
        
        return expr;
    }
    
    std::unique_ptr<Expression> parsePrimary() {
        if (check(TokenType::INTEGER)) {
            Token token = advance();
            int value = std::stoi(token.value);
            return std::make_unique<IntLiteral>(value, token.line, token.column);
        }
        
        if (check(TokenType::FLOAT)) {
            Token token = advance();
            double value = std::stod(token.value);
            return std::make_unique<FloatLiteral>(value, token.line, token.column);
        }
        
        if (check(TokenType::STRING)) {
            Token token = advance();
            
            // Check if string contains interpolation patterns
            if (token.value.find("${") != std::string::npos) {
                return parseInterpolatedString(token);
            } else {
                return std::make_unique<StringLiteral>(token.value, token.line, token.column);
            }
        }
        
        if (check(TokenType::TRUE) || check(TokenType::FALSE)) {
            Token token = advance();
            bool value = (token.value == "True");
            return std::make_unique<BoolLiteral>(value, token.line, token.column);
        }
        
        if (check(TokenType::LBRACKET)) {
            Token token = advance(); // consume '['
            auto list = std::make_unique<ListLiteral>(token.line, token.column);
            
            // Parse list elements
            if (!check(TokenType::RBRACKET)) {
                do {
                    // Use parseLogicalOr instead of parseExpression to avoid infinite recursion
                    list->elements.push_back(parseLogicalOr());
                } while (check(TokenType::COMMA) && (advance(), true));
            }
            
            if (!check(TokenType::RBRACKET)) {
                throw std::runtime_error("Expected ']' after list elements");
            }
            advance(); // consume ']'
            return std::move(list);
        }
        
        if (check(TokenType::LBRACE)) {
            Token token = advance(); // consume '{'
            auto dict = std::make_unique<DictLiteral>(token.line, token.column);
            
            // Parse dictionary key-value pairs
            if (!check(TokenType::RBRACE)) {
                do {
                    // Parse key
                    auto key = parseLogicalOr();
                    
                    // Expect colon
                    if (!check(TokenType::COLON)) {
                        throw std::runtime_error("Expected ':' after dictionary key");
                    }
                    advance(); // consume ':'
                    
                    // Parse value
                    auto value = parseLogicalOr();
                    
                    dict->keys.push_back(std::move(key));
                    dict->values.push_back(std::move(value));
                } while (check(TokenType::COMMA) && (advance(), true));
            }
            
            if (!check(TokenType::RBRACE)) {
                throw std::runtime_error("Expected '}' after dictionary elements");
            }
            advance(); // consume '}'
            return std::move(dict);
        }
        
        if (check(TokenType::LPAREN)) {
            advance(); // consume '('
            
            // Check if this is a tuple or just a parenthesized expression
            auto firstExpr = parseExpression();
            
            if (check(TokenType::COMMA)) {
                // This is a tuple
                auto tuple = std::make_unique<TupleExpression>();
                tuple->elements.push_back(std::move(firstExpr));
                
                do {
                    advance(); // consume ','
                    tuple->elements.push_back(parseExpression());
                } while (check(TokenType::COMMA));
                
                if (!check(TokenType::RPAREN)) {
                    throw std::runtime_error("Expected ')' after tuple");
                }
                advance(); // consume ')'
                return std::move(tuple);
            } else {
                // Just a parenthesized expression
                if (!check(TokenType::RPAREN)) {
                    throw std::runtime_error("Expected ')' after expression");
                }
                advance(); // consume ')'
                return firstExpr;
            }
        }
        
        // Handle built-in type conversion functions that are keywords
        if (check(TokenType::INT) && checkNext(TokenType::LPAREN)) {
            advance(); // consume 'int'
            return parseBuiltinFunctionCall("int");
        }
        
        if (check(TokenType::IDENTIFIER)) {
            std::string varName = advance().value;
            return std::make_unique<Identifier>(varName);
        }
        
        // Check if we've encountered a statement starter - if so, stop parsing expression
        if (isStatementStarter(peek().type)) {
            throw std::runtime_error("Unexpected " + peek().value + " in expression context");
        }
        
        throw std::runtime_error("Unexpected token in expression");
    }
    
    std::unique_ptr<FunctionCall> parseBuiltinFunctionCall(const std::string& name) {
        auto call = std::make_unique<FunctionCall>(name);
        
        advance(); // consume '('
        
        // Parse arguments
        if (!check(TokenType::RPAREN)) {
            do {
                call->arguments.push_back(parseExpression());
            } while (check(TokenType::COMMA) && (advance(), true));
        }
        
        if (!check(TokenType::RPAREN)) {
            throw std::runtime_error("Expected ')' after function arguments");
        }
        advance(); // consume ')'
        
        return call;
    }
    
    std::unique_ptr<InterpolatedString> parseInterpolatedString(const Token& token) {
        auto interpolated = std::make_unique<InterpolatedString>(token.line, token.column);
        std::string content = token.value;
        
        size_t pos = 0;
        while (pos < content.length()) {
            // Find next interpolation pattern
            size_t dollarPos = content.find("${", pos);
            
            if (dollarPos == std::string::npos) {
                // No more interpolation, add remaining text if any
                if (pos < content.length()) {
                    std::string textPart = content.substr(pos);
                    if (!textPart.empty()) {
                        interpolated->parts.emplace_back(textPart);
                    }
                }
                break;
            }
            
            // Add text before interpolation if any
            if (dollarPos > pos) {
                std::string textPart = content.substr(pos, dollarPos - pos);
                interpolated->parts.emplace_back(textPart);
            }
            
            // Find closing brace
            size_t bracePos = content.find("}", dollarPos + 2);
            if (bracePos == std::string::npos) {
                throw std::runtime_error("Missing closing '}' in string interpolation");
            }
            
            // Extract variable name
            std::string varName = content.substr(dollarPos + 2, bracePos - dollarPos - 2);
            if (varName.empty()) {
                throw std::runtime_error("Empty variable name in string interpolation");
            }
            
            // Create identifier expression for the variable
            auto varExpr = std::make_unique<Identifier>(varName, token.line, token.column);
            interpolated->parts.emplace_back(std::move(varExpr));
            
            pos = bracePos + 1;
        }
        
        return interpolated;
    }
    
    std::unique_ptr<WhileStatement> parseWhileStatement() {
        advance(); // consume 'while'
        
        // Parse condition
        auto condition = parseExpression();
        
        // Expect opening brace
        if (!check(TokenType::LBRACE)) {
            throw std::runtime_error("Expected '{' after while condition");
        }
        advance(); // consume '{'
        
        // Parse body (statements until '}')
        auto body = std::make_unique<BlockStatement>();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (check(TokenType::NEWLINE)) {
                advance();
                continue;
            }
            
            auto stmt = parseStatement();
            if (stmt) {
                body->statements.push_back(std::move(stmt));
            }
        }
        
        if (!check(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after while block");
        }
        advance(); // consume '}'
        
        return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
    }
    
    std::unique_ptr<Statement> parseForStatement() {
        advance(); // consume 'for'
        
        // Only support Python-style for-in loops: for variable in iterable { ... }
        if (!check(TokenType::IDENTIFIER)) {
            throw std::runtime_error("Expected variable name after 'for' in for-in loop");
        }
        
        std::string variable = advance().value; // consume variable name
        
        if (!check(TokenType::IN)) {
            throw std::runtime_error("Expected 'in' after variable in for-in loop. C-style for loops are not supported.");
        }
        advance(); // consume 'in'
        
        auto iterable = parseExpression();
        
        // Expect opening brace
        if (!check(TokenType::LBRACE)) {
            throw std::runtime_error("Expected '{' after for-in clause");
        }
        advance(); // consume '{'
        
        // Parse body
        auto body = std::make_unique<BlockStatement>();
        while (!check(TokenType::RBRACE) && !isAtEnd()) {
            if (check(TokenType::NEWLINE)) {
                advance();
                continue;
            }
            
            auto stmt = parseStatement();
            if (stmt) {
                body->statements.push_back(std::move(stmt));
            }
        }
        
        if (!check(TokenType::RBRACE)) {
            throw std::runtime_error("Expected '}' after for-in block");
        }
        advance(); // consume '}'
        
        return std::make_unique<ForInStatement>(variable, std::move(iterable), std::move(body));
    }
    
    // Helper methods for parameter parsing
    bool match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }
    
    Type parseType() {
        if (check(TokenType::INT)) {
            advance();
            return Type(TypeKind::INT32);
        }
        if (check(TokenType::INT64)) {
            advance();
            return Type(TypeKind::INT64);
        }
        if (check(TokenType::FLOAT32)) {
            advance();
            return Type(TypeKind::FLOAT32);
        }
        if (check(TokenType::FLOAT64)) {
            advance();
            return Type(TypeKind::FLOAT64);
        }
        if (check(TokenType::STRING_TYPE)) {
            advance();
            return Type(TypeKind::STRING);
        }
        if (check(TokenType::BOOL_TYPE)) {
            advance();
            return Type(TypeKind::BOOL);
        }
        if (check(TokenType::VOID)) {
            advance();
            return Type(TypeKind::VOID);
        }
        
        if (check(TokenType::IDENTIFIER)) {
            std::string name = advance().value;
            return Type(TypeKind::STRUCT, name); // Could be struct or enum
        }
        
        throw std::runtime_error("Expected type");
    }
    
    bool isTypeToken(const Token& token) const {
        return token.type == TokenType::INT || token.type == TokenType::INT64 ||
               token.type == TokenType::FLOAT32 || token.type == TokenType::FLOAT64 ||
               token.type == TokenType::STRING_TYPE || token.type == TokenType::BOOL_TYPE ||
               token.type == TokenType::VOID || token.type == TokenType::IDENTIFIER;
    }
};

} // namespace orion

#endif // SIMPLE_PARSER_H