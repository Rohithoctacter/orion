#include "ast.h"
#include <unordered_map>
#include <string>
#include <iostream>
#include <unordered_set>

namespace orion {

// Scoping system for LEGB resolution
class ScopeManager {
struct Scope {
    std::unordered_map<std::string, Type> variables;
    std::unordered_set<std::string> globalVars;  // declared global in this scope
    std::unordered_set<std::string> localVars;   // declared local in this scope
    std::unordered_set<std::string> constVars;   // declared const in this scope
    bool isFunction = false;
};

std::vector<Scope> scopeStack;
std::unordered_map<std::string, Type> globalScope;

public:
    void enterScope(bool isFunction = false) {
        Scope newScope;
        newScope.isFunction = isFunction;
        scopeStack.push_back(newScope);
    }
    
    void exitScope() {
        if (!scopeStack.empty()) {
            scopeStack.pop_back();
        }
    }
    
    void declareGlobal(const std::string& name) {
        if (!scopeStack.empty()) {
            scopeStack.back().globalVars.insert(name);
        }
    }
    
    void declareLocal(const std::string& name) {
        if (!scopeStack.empty()) {
            scopeStack.back().localVars.insert(name);
        }
    }
    
    void declareConst(const std::string& name) {
        if (!scopeStack.empty()) {
            scopeStack.back().constVars.insert(name);
        }
    }
    
    // LEGB resolution: Local -> Enclosing function -> Global
    Type* findVariable(const std::string& name) {
        // Check from innermost to outermost scope
        for (int i = scopeStack.size() - 1; i >= 0; i--) {
            const auto& scope = scopeStack[i];
            
            // If this scope declared the variable as global, look in global scope
            if (scope.globalVars.count(name)) {
                auto globalIt = globalScope.find(name);
                return (globalIt != globalScope.end()) ? &globalIt->second : nullptr;
            }
            
            // Check local variables in this scope
            auto localIt = scope.variables.find(name);
            if (localIt != scope.variables.end()) {
                return const_cast<Type*>(&localIt->second);
            }
        }
        
        // Check global scope as fallback
        auto globalIt = globalScope.find(name);
        return (globalIt != globalScope.end()) ? &globalIt->second : nullptr;
    }
    
    void setVariable(const std::string& name, const Type& type) {
        if (scopeStack.empty()) {
            // No scope, set in global
            globalScope[name] = type;
            return;
        }
        
        const auto& currentScope = scopeStack.back();
        
        // If declared as global in current scope, set in global scope
        if (currentScope.globalVars.count(name)) {
            globalScope[name] = type;
            return;
        }
        
        // Python-style scoping rules:
        // 1. If explicitly declared local, keep it local
        // 2. If we're in a function scope and no explicit declaration, create local variable
        // 3. If not in function scope, set in global scope
        
        if (currentScope.localVars.count(name) || 
            (currentScope.isFunction && !currentScope.globalVars.count(name))) {
            // Set in current local scope (explicit local or implicit local in function)
            scopeStack.back().variables[name] = type;
        } else {
            // Set in global scope (not in function or not declared as local/global)
            globalScope[name] = type;
        }
    }
    
    bool isGlobal() const {
        return scopeStack.empty();
    }
    
    bool isConst(const std::string& name) const {
        // Check from innermost to outermost scope
        for (int i = scopeStack.size() - 1; i >= 0; i--) {
            const auto& scope = scopeStack[i];
            if (scope.constVars.count(name)) {
                return true;
            }
        }
        return false;
    }
};

// Type variable system for inference
class TypeVariable {
public:
    std::string id;
    Type resolvedType;
    bool isResolved;
    std::string functionName;
    std::string parameterName;
    
    TypeVariable() : resolvedType(TypeKind::UNKNOWN), isResolved(false) {}
    
    TypeVariable(const std::string& funcName, const std::string& paramName) 
        : id(funcName + "::" + paramName), resolvedType(TypeKind::UNKNOWN), 
          isResolved(false), functionName(funcName), parameterName(paramName) {}
};

// Type constraint for inference
struct TypeConstraint {
    std::string typeVarId;
    Type constraintType;
    std::string reason;
    int line;
    
    TypeConstraint(const std::string& varId, const Type& type, const std::string& description, int lineNum = 0)
        : typeVarId(varId), constraintType(type), reason(description), line(lineNum) {}
};

class TypeChecker : public ASTVisitor {
private:
    ScopeManager scopeManager;
    std::unordered_map<std::string, FunctionDeclaration*> functions;
    std::unordered_map<std::string, StructDeclaration*> structs;
    std::unordered_map<std::string, EnumDeclaration*> enums;
    
    // Type inference system
    std::unordered_map<std::string, TypeVariable> typeVariables;
    std::vector<TypeConstraint> constraints;
    std::string currentFunctionName;
    int inferenceMaxIterations = 5;
    
    Type currentReturnType;
    std::vector<std::string> errors;
    std::vector<std::string> sourceLines;
    
public:
    bool check(Program& program, const std::vector<std::string>& srcLines = {}) {
        errors.clear();
        sourceLines = srcLines;
        typeVariables.clear();
        constraints.clear();
        
        // First pass: collect function, struct, and enum declarations
        for (auto& stmt : program.statements) {
            if (auto func = dynamic_cast<FunctionDeclaration*>(stmt.get())) {
                functions[func->name] = func;
                // Create type variables for implicit parameters
                createTypeVariablesForFunction(*func);
            } else if (auto structDecl = dynamic_cast<StructDeclaration*>(stmt.get())) {
                structs[structDecl->name] = structDecl;
            } else if (auto enumDecl = dynamic_cast<EnumDeclaration*>(stmt.get())) {
                enums[enumDecl->name] = enumDecl;
            }
        }
        
        // Second pass: type check and gather constraints
        program.accept(*this);
        
        // Third pass: perform type inference
        if (!performTypeInference()) {
            return false;
        }
        
        // Fourth pass: validate all types are resolved
        if (!validateResolvedTypes()) {
            return false;
        }
        
        if (!errors.empty()) {
            std::cerr << "Type checking errors:\n";
            for (const auto& error : errors) {
                std::cerr << "  " << error << "\n";
            }
            return false;
        }
        
        return true;
    }
    
    const std::vector<std::string>& getErrors() const {
        return errors;
    }
    
private:
    void addError(const std::string& message, int line = 0) {
        std::string fullMessage = message;
        if (line > 0) {
            fullMessage = "Line " + std::to_string(line) + ": " + message;
            if (line <= (int)sourceLines.size() && line > 0) {
                fullMessage += "\n    " + sourceLines[line - 1];
                // Add a caret to point to the error column if needed
                fullMessage += "\n    ^";
            }
        }
        errors.push_back(fullMessage);
    }
    
    void createTypeVariablesForFunction(FunctionDeclaration& func) {
        for (auto& param : func.parameters) {
            if (!param.isExplicitType || param.type.kind == TypeKind::UNKNOWN) {
                TypeVariable typeVar(func.name, param.name);
                typeVariables.emplace(typeVar.id, typeVar);
            }
        }
    }
    
    void addConstraint(const std::string& typeVarId, const Type& constraintType, const std::string& reason, int line = 0) {
        constraints.emplace_back(typeVarId, constraintType, reason, line);
    }
    
    bool performTypeInference() {
        bool changed = true;
        int iteration = 0;
        
        while (changed && iteration < inferenceMaxIterations) {
            changed = false;
            iteration++;
            
            // Unify constraints with type variables
            for (const auto& constraint : constraints) {
                auto typeVarIt = typeVariables.find(constraint.typeVarId);
                if (typeVarIt != typeVariables.end()) {
                    TypeVariable& typeVar = typeVarIt->second;
                    
                    if (!typeVar.isResolved) {
                        if (constraint.constraintType.kind != TypeKind::UNKNOWN) {
                            typeVar.resolvedType = constraint.constraintType;
                            typeVar.isResolved = true;
                            changed = true;
                            
                            // Update the actual parameter type in the function
                            updateParameterType(typeVar.functionName, typeVar.parameterName, constraint.constraintType);
                        }
                    } else {
                        // Check for type conflicts
                        if (!isCompatible(typeVar.resolvedType, constraint.constraintType)) {
                            addError("Type conflict for parameter '" + typeVar.parameterName + "' in function '" + 
                                   typeVar.functionName + "': inferred " + typeVar.resolvedType.toString() + 
                                   " but also used as " + constraint.constraintType.toString() + " (" + 
                                   constraint.reason + ")", constraint.line);
                            return false;
                        }
                    }
                }
            }
            
            // Propagate types between function calls
            if (propagateTypesAcrossCalls()) {
                changed = true;
            }
        }
        
        if (iteration >= inferenceMaxIterations) {
            addError("Type inference failed to converge within " + std::to_string(inferenceMaxIterations) + " iterations");
            return false;
        }
        
        return true;
    }
    
    bool propagateTypesAcrossCalls() {
        bool changed = false;
        
        // For each function call, unify argument types with parameter types
        for (const auto& funcPair : functions) {
            FunctionDeclaration* func = funcPair.second;
            
            // Find calls to this function and unify types
            // This is a simplified version - in practice would need to traverse call sites
            for (size_t i = 0; i < func->parameters.size(); i++) {
                const Parameter& param = func->parameters[i];
                std::string typeVarId = func->name + "::" + param.name;
                
                auto typeVarIt = typeVariables.find(typeVarId);
                if (typeVarIt != typeVariables.end() && !typeVarIt->second.isResolved) {
                    // Look for constraints that might resolve this parameter
                    for (const auto& constraint : constraints) {
                        if (constraint.typeVarId == typeVarId && constraint.constraintType.kind != TypeKind::UNKNOWN) {
                            typeVarIt->second.resolvedType = constraint.constraintType;
                            typeVarIt->second.isResolved = true;
                            updateParameterType(func->name, param.name, constraint.constraintType);
                            changed = true;
                            break;
                        }
                    }
                }
            }
        }
        
        return changed;
    }
    
    void updateParameterType(const std::string& functionName, const std::string& paramName, const Type& newType) {
        auto funcIt = functions.find(functionName);
        if (funcIt != functions.end()) {
            FunctionDeclaration* func = funcIt->second;
            for (auto& param : func->parameters) {
                if (param.name == paramName) {
                    param.type = newType;
                    break;
                }
            }
        }
    }
    
    bool validateResolvedTypes() {
        for (const auto& typeVarPair : typeVariables) {
            const TypeVariable& typeVar = typeVarPair.second;
            if (!typeVar.isResolved) {
                addError("Could not infer type for parameter '" + typeVar.parameterName + 
                        "' in function '" + typeVar.functionName + "'. " +
                        "Parameter is not used in function body or insufficient context for inference. " +
                        "Please add an explicit type annotation.");
                return false;
            }
        }
        return true;
    }
    
    void gatherArithmeticConstraints(Expression& expr, const Type& exprType) {
        if (auto id = dynamic_cast<Identifier*>(&expr)) {
            // Check if this is a parameter that needs type inference
            std::string typeVarId = currentFunctionName + "::" + id->name;
            if (typeVariables.find(typeVarId) != typeVariables.end()) {
                // This parameter is used in arithmetic, so it should be numeric
                // We'll prefer int32 as default numeric type
                addConstraint(typeVarId, Type(TypeKind::INT32), "used in arithmetic operation", expr.line);
            }
        }
    }
    
    void gatherComparisonConstraints(Expression& left, const Type& leftType, Expression& right, const Type& rightType) {
        // For comparisons, we need both sides to be compatible
        if (auto leftId = dynamic_cast<Identifier*>(&left)) {
            std::string typeVarId = currentFunctionName + "::" + leftId->name;
            if (typeVariables.find(typeVarId) != typeVariables.end()) {
                if (rightType.kind != TypeKind::UNKNOWN) {
                    addConstraint(typeVarId, rightType, "compared with " + rightType.toString(), left.line);
                }
            }
        }
        
        if (auto rightId = dynamic_cast<Identifier*>(&right)) {
            std::string typeVarId = currentFunctionName + "::" + rightId->name;
            if (typeVariables.find(typeVarId) != typeVariables.end()) {
                if (leftType.kind != TypeKind::UNKNOWN) {
                    addConstraint(typeVarId, leftType, "compared with " + leftType.toString(), right.line);
                }
            }
        }
    }
    
    Type inferType(Expression& expr) {
        // Simple type inference
        if (auto intLit = dynamic_cast<IntLiteral*>(&expr)) {
            return Type(TypeKind::INT32);
        }
        if (auto floatLit = dynamic_cast<FloatLiteral*>(&expr)) {
            return Type(TypeKind::FLOAT32);
        }
        if (auto stringLit = dynamic_cast<StringLiteral*>(&expr)) {
            return Type(TypeKind::STRING);
        }
        if (auto boolLit = dynamic_cast<BoolLiteral*>(&expr)) {
            return Type(TypeKind::BOOL);
        }
        if (auto id = dynamic_cast<Identifier*>(&expr)) {
            Type* varType = scopeManager.findVariable(id->name);
            if (varType) {
                return *varType;
            }
            addError("Undefined variable: " + id->name);
            return Type(TypeKind::UNKNOWN);
        }
        if (auto binExpr = dynamic_cast<BinaryExpression*>(&expr)) {
            return inferBinaryType(*binExpr);
        }
        if (auto call = dynamic_cast<FunctionCall*>(&expr)) {
            // Check for built-in type conversion functions
            if (call->name == "str") {
                return Type(TypeKind::STRING);
            }
            if (call->name == "int") {
                return Type(TypeKind::INT32);
            }
            if (call->name == "flt") {
                return Type(TypeKind::FLOAT32);
            }
            
            auto it = functions.find(call->name);
            if (it != functions.end()) {
                return it->second->returnType;
            }
            addError("Undefined function: " + call->name);
            return Type(TypeKind::UNKNOWN);
        }
        if (auto listLit = dynamic_cast<ListLiteral*>(&expr)) {
            return inferListType(*listLit);
        }
        if (auto indexExpr = dynamic_cast<IndexExpression*>(&expr)) {
            return inferIndexType(*indexExpr);
        }
        
        return Type(TypeKind::UNKNOWN);
    }
    
    Type inferBinaryType(BinaryExpression& expr) {
        Type leftType = inferType(*expr.left);
        Type rightType = inferType(*expr.right);
        
        // Arithmetic operations
        if (expr.op == BinaryOp::ADD || expr.op == BinaryOp::SUB ||
            expr.op == BinaryOp::MUL || expr.op == BinaryOp::DIV || expr.op == BinaryOp::MOD) {
            
            if (leftType.kind == TypeKind::STRING || rightType.kind == TypeKind::STRING) {
                if (expr.op == BinaryOp::ADD) {
                    return Type(TypeKind::STRING); // String concatenation
                } else {
                    addError("Invalid operation on string");
                    return Type(TypeKind::UNKNOWN);
                }
            }
            
            if (leftType.kind == TypeKind::FLOAT32 || rightType.kind == TypeKind::FLOAT32 ||
                leftType.kind == TypeKind::FLOAT64 || rightType.kind == TypeKind::FLOAT64) {
                return Type(TypeKind::FLOAT32);
            }
            
            if (leftType.kind == TypeKind::INT32 || leftType.kind == TypeKind::INT64 ||
                rightType.kind == TypeKind::INT32 || rightType.kind == TypeKind::INT64) {
                return Type(TypeKind::INT32);
            }
            
            addError("Invalid types for arithmetic operation");
            return Type(TypeKind::UNKNOWN);
        }
        
        // Comparison operations
        if (expr.op == BinaryOp::EQ || expr.op == BinaryOp::NE ||
            expr.op == BinaryOp::LT || expr.op == BinaryOp::LE ||
            expr.op == BinaryOp::GT || expr.op == BinaryOp::GE) {
            return Type(TypeKind::BOOL);
        }
        
        // Logical operations
        if (expr.op == BinaryOp::AND || expr.op == BinaryOp::OR) {
            if (leftType.kind != TypeKind::BOOL || rightType.kind != TypeKind::BOOL) {
                addError("Logical operations require boolean operands");
            }
            return Type(TypeKind::BOOL);
        }
        
        return Type(TypeKind::UNKNOWN);
    }
    
    bool isCompatible(const Type& expected, const Type& actual) {
        if (expected.kind == TypeKind::UNKNOWN || actual.kind == TypeKind::UNKNOWN) {
            return true; // Allow unknown types (for error recovery)
        }
        
        if (expected.kind == actual.kind) {
            // For LIST types, recursively check element types
            if (expected.kind == TypeKind::LIST) {
                if (!expected.elementType || !actual.elementType) {
                    return true; // Allow if either has unknown element type
                }
                return isCompatible(*expected.elementType, *actual.elementType);
            }
            return true;
        }
        
        // Allow some implicit conversions
        if ((expected.kind == TypeKind::FLOAT32 || expected.kind == TypeKind::FLOAT64) &&
            (actual.kind == TypeKind::INT32 || actual.kind == TypeKind::INT64)) {
            return true; // int to float
        }
        
        if (expected.kind == TypeKind::INT64 && actual.kind == TypeKind::INT32) {
            return true; // int32 to int64
        }
        
        return false;
    }
    
    // Type unification for lists - handles order-independent type promotion
    Type unifyTypes(const Type& type1, const Type& type2) {
        if (type1.kind == TypeKind::UNKNOWN) {
            return type2;
        }
        if (type2.kind == TypeKind::UNKNOWN) {
            return type1;
        }
        
        // If types are identical, return either one
        if (type1.kind == type2.kind) {
            if (type1.kind == TypeKind::LIST) {
                // Recursively unify element types
                if (!type1.elementType || !type2.elementType) {
                    Type result(TypeKind::LIST);
                    result.elementType = std::make_unique<Type>(TypeKind::UNKNOWN);
                    return result;
                }
                Type unifiedElement = unifyTypes(*type1.elementType, *type2.elementType);
                Type result(TypeKind::LIST);
                result.elementType = std::make_unique<Type>(unifiedElement);
                return result;
            }
            return type1;
        }
        
        // Handle numeric type promotion (order-independent)
        if ((type1.kind == TypeKind::INT32 || type1.kind == TypeKind::INT64) &&
            (type2.kind == TypeKind::FLOAT32 || type2.kind == TypeKind::FLOAT64)) {
            return type2; // promote int to float
        }
        if ((type2.kind == TypeKind::INT32 || type2.kind == TypeKind::INT64) &&
            (type1.kind == TypeKind::FLOAT32 || type1.kind == TypeKind::FLOAT64)) {
            return type1; // promote int to float
        }
        
        if (type1.kind == TypeKind::INT32 && type2.kind == TypeKind::INT64) {
            return type2; // promote int32 to int64
        }
        if (type2.kind == TypeKind::INT32 && type1.kind == TypeKind::INT64) {
            return type1; // promote int32 to int64
        }
        
        // Types cannot be unified
        return Type(TypeKind::UNKNOWN);
    }
    
    Type inferListType(ListLiteral& listLit) {
        if (listLit.elements.empty()) {
            // Empty list - return generic list type
            Type listType(TypeKind::LIST);
            listType.elementType = std::make_unique<Type>(TypeKind::UNKNOWN);
            return listType;
        }
        
        // Start with the first element's type
        Type unifiedType = inferType(*listLit.elements[0]);
        
        // Unify types of all elements (order-independent)
        for (size_t i = 1; i < listLit.elements.size(); i++) {
            Type elemType = inferType(*listLit.elements[i]);
            Type newUnified = unifyTypes(unifiedType, elemType);
            
            if (newUnified.kind == TypeKind::UNKNOWN) {
                addError("List elements must have compatible types: cannot unify " + 
                        unifiedType.toString() + " and " + elemType.toString());
                return Type(TypeKind::UNKNOWN);
            }
            
            unifiedType = newUnified;
        }
        
        // Create list type with unified element type
        Type listType(TypeKind::LIST);
        listType.elementType = std::make_unique<Type>(unifiedType);
        return listType;
    }
    
    Type inferIndexType(IndexExpression& indexExpr) {
        Type objectType = inferType(*indexExpr.object);
        Type indexType = inferType(*indexExpr.index);
        
        // Check that index is integer
        if (indexType.kind != TypeKind::INT32 && indexType.kind != TypeKind::INT64) {
            addError("List index must be an integer, got " + indexType.toString());
            return Type(TypeKind::UNKNOWN);
        }
        
        // Check that object is a list
        if (objectType.kind != TypeKind::LIST) {
            addError("Cannot index non-list type " + objectType.toString());
            return Type(TypeKind::UNKNOWN);
        }
        
        // Return element type
        if (objectType.elementType) {
            return *objectType.elementType;
        } else {
            return Type(TypeKind::UNKNOWN);
        }
    }
    
public:
    void visit(IntLiteral& node) override {}
    void visit(FloatLiteral& node) override {}
    void visit(StringLiteral& node) override {}
    void visit(BoolLiteral& node) override {}
    void visit(Identifier& node) override {
        Type* varType = scopeManager.findVariable(node.name);
        if (!varType) {
            addError("Undefined variable: " + node.name, node.line);
        }
    }
    
    void visit(BinaryExpression& node) override {
        node.left->accept(*this);
        node.right->accept(*this);
        
        // Gather constraints for type inference
        Type leftType = inferType(*node.left);
        Type rightType = inferType(*node.right);
        
        // For arithmetic operations, both operands should be numeric
        if (node.op == BinaryOp::ADD || node.op == BinaryOp::SUB || 
            node.op == BinaryOp::MUL || node.op == BinaryOp::DIV || 
            node.op == BinaryOp::MOD || node.op == BinaryOp::POWER || 
            node.op == BinaryOp::FLOOR_DIV) {
            
            gatherArithmeticConstraints(*node.left, leftType);
            gatherArithmeticConstraints(*node.right, rightType);
        }
        // For comparison operations, types should be compatible
        else if (node.op == BinaryOp::EQ || node.op == BinaryOp::NE ||
                 node.op == BinaryOp::LT || node.op == BinaryOp::LE ||
                 node.op == BinaryOp::GT || node.op == BinaryOp::GE) {
            
            gatherComparisonConstraints(*node.left, leftType, *node.right, rightType);
        }
    }
    
    void visit(UnaryExpression& node) override {
        node.operand->accept(*this);
    }
    
    void visit(TupleExpression& node) override {
        for (auto& element : node.elements) {
            element->accept(*this);
        }
    }
    
    void visit(ListLiteral& node) override {
        // Visit all elements for type checking
        for (auto& element : node.elements) {
            element->accept(*this);
        }
        // Note: inferListType is called implicitly in inferType when needed
    }
    
    void visit(DictLiteral& node) override {
        // Visit all keys and values for type checking
        for (auto& key : node.keys) {
            key->accept(*this);
        }
        for (auto& value : node.values) {
            value->accept(*this);
        }
        // Note: inferDictType is called implicitly in inferType when needed
    }
    
    void visit(IndexExpression& node) override {
        // Check object and index types
        node.object->accept(*this);
        node.index->accept(*this);
        // Note: inferIndexType is called implicitly in inferType when needed
    }
    
    void visit(FunctionCall& node) override {
        // Check for built-in type conversion functions first
        if (node.name == "str" || node.name == "int" || node.name == "flt") {
            // Built-in functions expect exactly one argument
            if (node.arguments.size() != 1) {
                addError("Built-in function " + node.name + "() expects 1 argument, got " +
                        std::to_string(node.arguments.size()));
                return;
            }
            
            // Visit the argument for type checking
            node.arguments[0]->accept(*this);
            Type argType = inferType(*node.arguments[0]);
            
            // Validate argument types for each built-in function
            if (node.name == "str") {
                // str() accepts int, float, bool, or string
                if (argType.kind != TypeKind::INT32 && argType.kind != TypeKind::INT64 &&
                    argType.kind != TypeKind::FLOAT32 && argType.kind != TypeKind::FLOAT64 &&
                    argType.kind != TypeKind::BOOL && argType.kind != TypeKind::STRING) {
                    addError("str() cannot convert " + argType.toString() + " to string");
                }
            } else if (node.name == "int") {
                // int() accepts int, float, bool, or string
                if (argType.kind != TypeKind::INT32 && argType.kind != TypeKind::INT64 &&
                    argType.kind != TypeKind::FLOAT32 && argType.kind != TypeKind::FLOAT64 &&
                    argType.kind != TypeKind::BOOL && argType.kind != TypeKind::STRING) {
                    addError("int() cannot convert " + argType.toString() + " to integer");
                }
            } else if (node.name == "flt") {
                // flt() accepts int, float, bool, or string
                if (argType.kind != TypeKind::INT32 && argType.kind != TypeKind::INT64 &&
                    argType.kind != TypeKind::FLOAT32 && argType.kind != TypeKind::FLOAT64 &&
                    argType.kind != TypeKind::BOOL && argType.kind != TypeKind::STRING) {
                    addError("flt() cannot convert " + argType.toString() + " to float");
                }
            }
            return;
        }
        
        auto it = functions.find(node.name);
        if (it == functions.end()) {
            addError("Undefined function: " + node.name);
            return;
        }
        
        FunctionDeclaration* func = it->second;
        
        // Visit arguments first
        for (auto& arg : node.arguments) {
            arg->accept(*this);
        }
        
        // Check argument count
        if (node.arguments.size() != func->parameters.size()) {
            addError("Function " + node.name + " expects " + 
                    std::to_string(func->parameters.size()) + " arguments, got " +
                    std::to_string(node.arguments.size()));
            return;
        }
        
        // Check argument types and gather constraints
        for (size_t i = 0; i < node.arguments.size(); i++) {
            Type argType = inferType(*node.arguments[i]);
            const Parameter& param = func->parameters[i];
            
            // If parameter needs type inference, add constraint from argument
            std::string paramTypeVarId = func->name + "::" + param.name;
            if (typeVariables.find(paramTypeVarId) != typeVariables.end()) {
                if (argType.kind != TypeKind::UNKNOWN) {
                    addConstraint(paramTypeVarId, argType, "argument " + std::to_string(i + 1) + 
                                 " in call to " + node.name, node.line);
                }
            }
            
            // If argument is a parameter from current function, add constraint
            if (auto argId = dynamic_cast<Identifier*>(node.arguments[i].get())) {
                std::string argTypeVarId = currentFunctionName + "::" + argId->name;
                if (typeVariables.find(argTypeVarId) != typeVariables.end()) {
                    if (param.isExplicitType && param.type.kind != TypeKind::UNKNOWN) {
                        addConstraint(argTypeVarId, param.type, "passed as argument " + 
                                     std::to_string(i + 1) + " to " + node.name, node.line);
                    }
                }
            }
            
            // Standard type checking
            if (param.isExplicitType && param.type.kind != TypeKind::UNKNOWN && 
                argType.kind != TypeKind::UNKNOWN && !isCompatible(param.type, argType)) {
                addError("Argument " + std::to_string(i + 1) + " to function " + node.name +
                        " has wrong type: expected " + param.type.toString() + 
                        ", got " + argType.toString());
            }
        }
    }
    
    void visit(VariableDeclaration& node) override {
        // Check const variable requirements
        if (node.isConstant && !node.initializer) {
            addError("Constant variable '" + node.name + "' must be initialized", node.line);
            return;
        }
        
        if (node.initializer) {
            node.initializer->accept(*this);
            
            Type initType = inferType(*node.initializer);
            
            if (!node.hasExplicitType) {
                // Type inference
                if (initType.kind == TypeKind::UNKNOWN) {
                    // Default to int32 if we can't infer
                    node.type = Type(TypeKind::INT32);
                } else {
                    node.type = initType;
                }
            } else {
                // Type checking
                if (!isCompatible(node.type, initType)) {
                    addError("Cannot assign " + initType.toString() + 
                            " to variable of type " + node.type.toString());
                }
            }
        } else if (!node.hasExplicitType) {
            addError("Variable " + node.name + " needs either explicit type or initializer");
        }
        
        // Mark as const if needed
        if (node.isConstant) {
            scopeManager.declareConst(node.name);
        }
        
        scopeManager.setVariable(node.name, node.type);
    }
    
    void visit(FunctionDeclaration& node) override {
        // Set up function context
        Type savedReturnType = currentReturnType;
        currentReturnType = node.returnType;
        std::string savedFunctionName = currentFunctionName;
        currentFunctionName = node.name;
        
        // Enter function scope
        scopeManager.enterScope(true);
        
        // Add parameters to function scope
        for (const auto& param : node.parameters) {
            Type paramType = param.type;
            
            // Explicitly declare parameters as local to ensure they go in function scope
            scopeManager.declareLocal(param.name);
            
            // For implicit parameters, still use the type (even if UNKNOWN) so they're in scope
            // The type inference will resolve UNKNOWN types later
            scopeManager.setVariable(param.name, paramType);
        }
        
        if (node.isSingleExpression) {
            // Single expression function
            node.expression->accept(*this);
            Type exprType = inferType(*node.expression);
            
            if (!isCompatible(node.returnType, exprType)) {
                addError("Function " + node.name + " returns " + exprType.toString() +
                        " but declared return type is " + node.returnType.toString());
            }
        } else {
            // Block function
            for (auto& stmt : node.body) {
                stmt->accept(*this);
            }
        }
        
        // Exit function scope
        scopeManager.exitScope();
        
        // Restore context
        currentReturnType = savedReturnType;
        currentFunctionName = savedFunctionName;
    }
    
    void visit(BlockStatement& node) override {
        for (auto& stmt : node.statements) {
            stmt->accept(*this);
        }
    }
    
    void visit(ExpressionStatement& node) override {
        node.expression->accept(*this);
    }
    
    void visit(GlobalStatement& node) override {
        for (const std::string& varName : node.variables) {
            scopeManager.declareGlobal(varName);
        }
    }
    
    void visit(LocalStatement& node) override {
        for (const std::string& varName : node.variables) {
            scopeManager.declareLocal(varName);
        }
    }
    
    void visit(ReturnStatement& node) override {
        if (node.value) {
            node.value->accept(*this);
            Type returnType = inferType(*node.value);
            
            if (!isCompatible(currentReturnType, returnType)) {
                addError("Return type mismatch: expected " + currentReturnType.toString() +
                        ", got " + returnType.toString());
            }
        } else if (currentReturnType.kind != TypeKind::VOID) {
            addError("Non-void function must return a value");
        }
    }
    
    void visit(IfStatement& node) override {
        node.condition->accept(*this);
        Type condType = inferType(*node.condition);
        
        if (condType.kind != TypeKind::BOOL) {
            addError("If condition must be boolean, got " + condType.toString());
        }
        
        node.thenBranch->accept(*this);
        if (node.elseBranch) {
            node.elseBranch->accept(*this);
        }
    }
    
    void visit(WhileStatement& node) override {
        node.condition->accept(*this);
        Type condType = inferType(*node.condition);
        
        if (condType.kind != TypeKind::BOOL) {
            addError("While condition must be boolean, got " + condType.toString());
        }
        
        node.body->accept(*this);
    }
    
    // ForStatement removed - only ForInStatement is supported
    
    void visit(StructDeclaration& node) override {
        // Basic validation - check for duplicate fields
        std::unordered_map<std::string, bool> fieldNames;
        for (const auto& field : node.fields) {
            if (fieldNames[field.name]) {
                addError("Duplicate field name in struct " + node.name + ": " + field.name);
            }
            fieldNames[field.name] = true;
        }
    }
    
    void visit(EnumDeclaration& node) override {
        // Basic validation - check for duplicate values
        std::unordered_map<std::string, bool> valueNames;
        for (const auto& value : node.values) {
            if (valueNames[value.name]) {
                addError("Duplicate value name in enum " + node.name + ": " + value.name);
            }
            valueNames[value.name] = true;
        }
    }
    
    void visit(Program& node) override {
        for (auto& stmt : node.statements) {
            stmt->accept(*this);
        }
    }
};

} // namespace orion
