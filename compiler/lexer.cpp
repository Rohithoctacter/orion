#include "lexer.h"
#include <iostream>
#include <cctype>
#include <sstream>

namespace orion {

std::string Token::typeToString() const {
    switch (type) {
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::BOOL: return "BOOL";
        case TokenType::INTERPOLATED_STRING_START: return "INTERPOLATED_STRING_START";
        case TokenType::INTERPOLATED_STRING_PART: return "INTERPOLATED_STRING_PART";
        case TokenType::INTERPOLATED_STRING_END: return "INTERPOLATED_STRING_END";
        case TokenType::INTERPOLATION_START: return "INTERPOLATION_START";
        case TokenType::INTERPOLATION_END: return "INTERPOLATION_END";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::IF: return "IF";
        case TokenType::ELIF: return "ELIF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::RETURN: return "RETURN";
        case TokenType::STRUCT: return "STRUCT";
        case TokenType::ENUM: return "ENUM";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::INT: return "INT";
        case TokenType::INT64: return "INT64";
        case TokenType::FLOAT32: return "FLOAT32";
        case TokenType::FLOAT64: return "FLOAT64";
        case TokenType::STRING_TYPE: return "STRING_TYPE";
        case TokenType::BOOL_TYPE: return "BOOL_TYPE";
        case TokenType::VOID: return "VOID";
        case TokenType::GLOBAL: return "GLOBAL";
        case TokenType::LOCAL: return "LOCAL";
        case TokenType::CONST: return "CONST";
        case TokenType::BREAK: return "BREAK";
        case TokenType::CONTINUE: return "CONTINUE";
        case TokenType::PASS: return "PASS";
        case TokenType::IN: return "IN";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MULTIPLY: return "MULTIPLY";
        case TokenType::DIVIDE: return "DIVIDE";
        case TokenType::MODULO: return "MODULO";
        case TokenType::POWER: return "POWER";
        case TokenType::FLOOR_DIVIDE: return "FLOOR_DIVIDE";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::PLUS_ASSIGN: return "PLUS_ASSIGN";
        case TokenType::MINUS_ASSIGN: return "MINUS_ASSIGN";
        case TokenType::MULTIPLY_ASSIGN: return "MULTIPLY_ASSIGN";
        case TokenType::DIVIDE_ASSIGN: return "DIVIDE_ASSIGN";
        case TokenType::MODULO_ASSIGN: return "MODULO_ASSIGN";
        case TokenType::EQ: return "EQ";
        case TokenType::NE: return "NE";
        case TokenType::LT: return "LT";
        case TokenType::LE: return "LE";
        case TokenType::GT: return "GT";
        case TokenType::GE: return "GE";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::ARROW: return "ARROW";
        case TokenType::FAT_ARROW: return "FAT_ARROW";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::EOF_TOKEN: return "EOF";
        default: return "INVALID";
    }
}

Lexer::Lexer(const std::string& src) : source(src), current(0), line(1), column(1) {}
    
std::vector<Token> Lexer::tokenize() {
        std::vector<Token> tokens;
        
        while (!isAtEnd()) {
            Token token = nextToken();
            if (token.type != TokenType::INVALID) {
                tokens.push_back(token);
            }
        }
        
        tokens.emplace_back(TokenType::EOF_TOKEN, "", line, column);
        return tokens;
    }

bool Lexer::isAtEnd() const {
        return current >= source.length();
    }

char Lexer::advance() {
        if (isAtEnd()) return '\0';
        char c = source[current++];
        if (c == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
        return c;
    }

char Lexer::peek() const {
        if (isAtEnd()) return '\0';
        return source[current];
    }

char Lexer::peekNext() const {
        if (current + 1 >= source.length()) return '\0';
        return source[current + 1];
    }

Token Lexer::nextToken() {
        skipWhitespace();
        
        if (isAtEnd()) {
            return Token(TokenType::EOF_TOKEN, "", line, column);
        }
        
        int tokenLine = line;
        int tokenColumn = column;
        char c = advance();
        
        // Single-line comments with #
        if (c == '#') {
            skipLineComment();
            return nextToken();
        }
        
        // Floor division operator //
        if (c == '/' && peek() == '/') {
            advance();
            return Token(TokenType::FLOOR_DIVIDE, "//", tokenLine, tokenColumn);
        }
        
        // Multi-line comments
        if (c == '/' && peek() == '*') {
            skipBlockComment();
            return nextToken();
        }
        
        // Numbers
        if (std::isdigit(c)) {
            return number(c, tokenLine, tokenColumn);
        }
        
        // Strings
        if (c == '"') {
            return string('"', tokenLine, tokenColumn);
        }
        if (c == '\'') {
            return string('\'', tokenLine, tokenColumn);
        }
        
        // Identifiers and keywords
        if (std::isalpha(c) || c == '_') {
            return identifier(c, tokenLine, tokenColumn);
        }
        
        // Two-character operators
        if (c == '=' && peek() == '=') {
            advance();
            return Token(TokenType::EQ, "==", tokenLine, tokenColumn);
        }
        if (c == '!' && peek() == '=') {
            advance();
            return Token(TokenType::NE, "!=", tokenLine, tokenColumn);
        }
        if (c == '<' && peek() == '=') {
            advance();
            return Token(TokenType::LE, "<=", tokenLine, tokenColumn);
        }
        if (c == '>' && peek() == '=') {
            advance();
            return Token(TokenType::GE, ">=", tokenLine, tokenColumn);
        }
        if (c == '&' && peek() == '&') {
            advance();
            return Token(TokenType::AND, "&&", tokenLine, tokenColumn);
        }
        if (c == '|' && peek() == '|') {
            advance();
            return Token(TokenType::OR, "||", tokenLine, tokenColumn);
        }
        if (c == '+' && peek() == '+') {
            advance();
            return Token(TokenType::INCREMENT, "++", tokenLine, tokenColumn);
        }
        if (c == '-' && peek() == '-') {
            advance();
            return Token(TokenType::DECREMENT, "--", tokenLine, tokenColumn);
        }
        if (c == '+' && peek() == '=') {
            advance();
            return Token(TokenType::PLUS_ASSIGN, "+=", tokenLine, tokenColumn);
        }
        if (c == '-' && peek() == '=') {
            advance();
            return Token(TokenType::MINUS_ASSIGN, "-=", tokenLine, tokenColumn);
        }
        if (c == '*' && peek() == '=') {
            advance();
            return Token(TokenType::MULTIPLY_ASSIGN, "*=", tokenLine, tokenColumn);
        }
        if (c == '/' && peek() == '=') {
            advance();
            return Token(TokenType::DIVIDE_ASSIGN, "/=", tokenLine, tokenColumn);
        }
        if (c == '%' && peek() == '=') {
            advance();
            return Token(TokenType::MODULO_ASSIGN, "%=", tokenLine, tokenColumn);
        }
        if (c == '-' && peek() == '>') {
            advance();
            return Token(TokenType::ARROW, "->", tokenLine, tokenColumn);
        }
        if (c == '=' && peek() == '>') {
            advance();
            return Token(TokenType::FAT_ARROW, "=>", tokenLine, tokenColumn);
        }
        if (c == '*' && peek() == '*') {
            advance();
            return Token(TokenType::POWER, "**", tokenLine, tokenColumn);
        }
        
        // Single-character tokens
        switch (c) {
            case '+': return Token(TokenType::PLUS, "+", tokenLine, tokenColumn);
            case '-': return Token(TokenType::MINUS, "-", tokenLine, tokenColumn);
            case '*': return Token(TokenType::MULTIPLY, "*", tokenLine, tokenColumn);
            case '/': return Token(TokenType::DIVIDE, "/", tokenLine, tokenColumn);
            case '%': return Token(TokenType::MODULO, "%", tokenLine, tokenColumn);
            case '=': return Token(TokenType::ASSIGN, "=", tokenLine, tokenColumn);
            case '<': return Token(TokenType::LT, "<", tokenLine, tokenColumn);
            case '>': return Token(TokenType::GT, ">", tokenLine, tokenColumn);
            case '!': return Token(TokenType::NOT, "!", tokenLine, tokenColumn);
            case ';': return Token(TokenType::SEMICOLON, ";", tokenLine, tokenColumn);
            case ',': return Token(TokenType::COMMA, ",", tokenLine, tokenColumn);
            case '.': return Token(TokenType::DOT, ".", tokenLine, tokenColumn);
            case ':': return Token(TokenType::COLON, ":", tokenLine, tokenColumn);
            case '(': return Token(TokenType::LPAREN, "(", tokenLine, tokenColumn);
            case ')': return Token(TokenType::RPAREN, ")", tokenLine, tokenColumn);
            case '{': return Token(TokenType::LBRACE, "{", tokenLine, tokenColumn);
            case '}': return Token(TokenType::RBRACE, "}", tokenLine, tokenColumn);
            case '[': return Token(TokenType::LBRACKET, "[", tokenLine, tokenColumn);
            case ']': return Token(TokenType::RBRACKET, "]", tokenLine, tokenColumn);
            case '\n': return Token(TokenType::NEWLINE, "\\n", tokenLine, tokenColumn);
            default:
                return Token(TokenType::INVALID, std::string(1, c), tokenLine, tokenColumn);
        }
    }

void Lexer::skipWhitespace() {
        while (!isAtEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r')) {
            advance();
        }
    }

void Lexer::skipLineComment() {
        while (!isAtEnd() && peek() != '\n') {
            advance();
        }
    }

void Lexer::skipBlockComment() {
        advance(); // skip '*'
        while (!isAtEnd()) {
            if (peek() == '*' && peekNext() == '/') {
                advance(); // skip '*'
                advance(); // skip '/'
                break;
            }
            advance();
        }
    }

Token Lexer::number(char first, int tokenLine, int tokenColumn) {
        std::string value(1, first);
        bool isFloat = false;
        
        while (!isAtEnd() && std::isdigit(peek())) {
            value += advance();
        }
        
        // Check for decimal point
        if (!isAtEnd() && peek() == '.' && std::isdigit(peekNext())) {
            isFloat = true;
            value += advance(); // consume '.'
            while (!isAtEnd() && std::isdigit(peek())) {
                value += advance();
            }
        }
        
        return Token(isFloat ? TokenType::FLOAT : TokenType::INTEGER, 
                    value, tokenLine, tokenColumn);
    }

Token Lexer::string(char quote, int tokenLine, int tokenColumn) {
        // First, scan the string to see if it contains interpolation
        size_t savePos = current;
        int saveLine = line;
        int saveColumn = column;
        bool hasInterpolation = false;
        
        while (!isAtEnd() && peek() != quote) {
            if (peek() == '$' && peekNext() == '{') {
                hasInterpolation = true;
                break;
            }
            if (peek() == '\\') {
                advance(); // skip escape
                if (!isAtEnd()) advance(); // skip escaped char
            } else {
                advance();
            }
        }
        
        // Reset position
        current = savePos;
        line = saveLine;
        column = saveColumn;
        
        // If no interpolation, handle as regular string
        if (!hasInterpolation) {
            std::string value;
            
            while (!isAtEnd() && peek() != quote) {
                char c = peek();
                if (c == '\\') {
                    advance(); // skip '\'
                    if (!isAtEnd()) {
                        char escaped = advance();
                        switch (escaped) {
                            case 'n': value += '\n'; break;
                            case 't': value += '\t'; break;
                            case 'r': value += '\r'; break;
                            case '\\': value += '\\'; break;
                            case '"': value += '"'; break;
                            case '\'': value += '\''; break;
                            default: value += escaped; break;
                        }
                    }
                } else {
                    value += advance();
                }
            }
            
            if (!isAtEnd()) {
                advance(); // consume closing quote
            }
            
            return Token(TokenType::STRING, value, tokenLine, tokenColumn);
        } else {
            // This is an interpolated string, but we need to handle it differently
            // For now, return a special token and handle interpolation in the parser
            std::string fullString;
            while (!isAtEnd() && peek() != quote) {
                fullString += advance();
            }
            if (!isAtEnd()) {
                advance(); // consume closing quote
            }
            
            // Return the full string with a special marker for interpolation
            return Token(TokenType::STRING, fullString, tokenLine, tokenColumn);
        }
    }

Token Lexer::identifier(char first, int tokenLine, int tokenColumn) {
        std::string value(1, first);
        
        while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
            value += advance();
        }
        
        // Check if it's a keyword
        auto it = keywords.find(value);
        TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;
        
        return Token(type, value, tokenLine, tokenColumn);
    }
std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"if", TokenType::IF},
    {"elif", TokenType::ELIF},
    {"else", TokenType::ELSE},
    {"while", TokenType::WHILE},
    {"for", TokenType::FOR},
    {"return", TokenType::RETURN},
    {"struct", TokenType::STRUCT},
    {"enum", TokenType::ENUM},
    {"import", TokenType::IMPORT},
    {"True", TokenType::TRUE},
    {"False", TokenType::FALSE},
    {"int", TokenType::INT},
    {"int64", TokenType::INT64},
    {"float", TokenType::FLOAT32},
    {"float64", TokenType::FLOAT64},
    {"string", TokenType::STRING_TYPE},
    {"bool", TokenType::BOOL_TYPE},
    {"void", TokenType::VOID},
    {"global", TokenType::GLOBAL},
    {"local", TokenType::LOCAL},
    {"const", TokenType::CONST},
    {"break", TokenType::BREAK},
    {"continue", TokenType::CONTINUE},
    {"pass", TokenType::PASS},
    {"in", TokenType::IN},
};

} // namespace orion
