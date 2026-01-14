#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include <unordered_map>

namespace orion {

enum class TokenType {
    // Literals
    INTEGER, FLOAT, STRING, BOOL,
    
    // String interpolation tokens
    INTERPOLATED_STRING_START, INTERPOLATED_STRING_PART, INTERPOLATED_STRING_END,
    INTERPOLATION_START, INTERPOLATION_END,
    
    // Identifiers and keywords
    IDENTIFIER,
    
    // Keywords
    IF, ELIF, ELSE, WHILE, FOR, RETURN,
    STRUCT, ENUM, IMPORT, TRUE, FALSE,
    INT, INT64, FLOAT32, FLOAT64, STRING_TYPE, BOOL_TYPE, VOID,
    GLOBAL, LOCAL, CONST,
    BREAK, CONTINUE, PASS, IN,
    
    // Operators
    PLUS, MINUS, MULTIPLY, DIVIDE, MODULO,
    POWER, FLOOR_DIVIDE,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, MULTIPLY_ASSIGN, DIVIDE_ASSIGN, MODULO_ASSIGN,
    EQ, NE, LT, LE, GT, GE,
    AND, OR, NOT,
    INCREMENT, DECREMENT,
    
    // Punctuation
    SEMICOLON, COMMA, DOT, COLON,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    ARROW, FAT_ARROW,
    
    // Special
    NEWLINE, EOF_TOKEN, INVALID
};

struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;
    
    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
    
    std::string typeToString() const;
};

class Lexer {
private:
    std::string source;
    size_t current;
    int line;
    int column;
    
    static std::unordered_map<std::string, TokenType> keywords;
    
public:
    Lexer(const std::string& src);
    std::vector<Token> tokenize();
    
private:
    bool isAtEnd() const;
    char advance();
    char peek() const;
    char peekNext() const;
    Token nextToken();
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    Token number(char first, int tokenLine, int tokenColumn);
    Token string(char quote, int tokenLine, int tokenColumn);
    std::vector<Token> interpolatedString(char quote, int tokenLine, int tokenColumn);
    Token identifier(char first, int tokenLine, int tokenColumn);
};

} // namespace orion

#endif // LEXER_H