#include "flux/CodeGen.h"

#include <stdexcept>

namespace flux {

uint32_t ConstantPool::addString(const std::string& s) {
    for (size_t i = 0; i < strings.size(); ++i) {
        if (strings[i] == s) return i;
    }
    strings.push_back(s);
    return strings.size() - 1;
}

uint32_t ConstantPool::addName(const std::string& name) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) return i;
    }
    names.push_back(name);
    return names.size() - 1;
}

uint32_t ConstantPool::addNumber(double num) {
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (numbers[i] == num) return i;
    }
    numbers.push_back(num);
    return numbers.size() - 1;
}

CodeGen::CodeGen() : scopeDepth_(0) {}

std::vector<Instruction> CodeGen::generate(const Program& program) {
    code_.clear();
    constantPool_ = ConstantPool();
    functions_.clear();
    nameToIndex_.clear();
    stringToIndex_.clear();
    numberToIndex_.clear();
    scopeDepth_ = 0;

    // Generate code for each statement
    for (const auto& stmt : program.statements) {
        generateStmt(*stmt);
    }

    // Add halt at the end
    emit(Instruction::HALT);

    return code_;
}

void CodeGen::emit(const Instruction& instr) {
    code_.push_back(instr);
}

void CodeGen::emit(Instruction::OpCode op, uint32_t o1, uint32_t o2, double numOp) {
    code_.push_back(Instruction(op, o1, o2, numOp));
}

uint32_t CodeGen::currentAddress() const {
    return static_cast<uint32_t>(code_.size());
}

void CodeGen::patchJump(uint32_t addr, uint32_t target) {
    if (addr < code_.size()) {
        code_[addr].operand1 = target;
    }
}

uint32_t CodeGen::getNameIndex(const std::string& name) {
    if (nameToIndex_.find(name) == nameToIndex_.end()) {
        nameToIndex_[name] = constantPool_.addName(name);
    }
    return nameToIndex_[name];
}

uint32_t CodeGen::getStringIndex(const std::string& str) {
    if (stringToIndex_.find(str) == stringToIndex_.end()) {
        stringToIndex_[str] = constantPool_.addString(str);
    }
    return stringToIndex_[str];
}

void CodeGen::generateStmt(const Stmt& stmt) {
    if (auto exprStmt = dynamic_cast<const ExpressionStmt*>(&stmt)) {
        generateExpressionStmt(*exprStmt);
    } else if (auto assignStmt = dynamic_cast<const AssignmentStmt*>(&stmt)) {
        generateAssignmentStmt(*assignStmt);
    } else if (auto showStmt = dynamic_cast<const ShowStmt*>(&stmt)) {
        generateShowStmt(*showStmt);
    } else if (auto giveStmt = dynamic_cast<const GiveStmt*>(&stmt)) {
        generateGiveStmt(*giveStmt);
    } else if (auto rewindStmt = dynamic_cast<const RewindStmt*>(&stmt)) {
        generateRewindStmt(*rewindStmt);
    } else if (auto blockStmt = dynamic_cast<const BlockStmt*>(&stmt)) {
        generateBlockStmt(*blockStmt);
    } else if (auto whenStmt = dynamic_cast<const WhenStmt*>(&stmt)) {
        generateWhenStmt(*whenStmt);
    } else if (auto repeatStmt = dynamic_cast<const RepeatStmt*>(&stmt)) {
        generateRepeatStmt(*repeatStmt);
    } else if (auto untilStmt = dynamic_cast<const UntilStmt*>(&stmt)) {
        generateUntilStmt(*untilStmt);
    } else if (auto eachStmt = dynamic_cast<const EachStmt*>(&stmt)) {
        generateEachStmt(*eachStmt);
    } else if (auto fnStmt = dynamic_cast<const FunctionStmt*>(&stmt)) {
        generateFunctionStmt(*fnStmt);
    }
}

void CodeGen::generateExpr(const Expr& expr) {
    if (auto binExpr = dynamic_cast<const BinaryExpr*>(&expr)) {
        generateBinaryExpr(*binExpr);
    } else if (auto unaryExpr = dynamic_cast<const UnaryExpr*>(&expr)) {
        generateUnaryExpr(*unaryExpr);
    } else if (auto callExpr = dynamic_cast<const CallExpr*>(&expr)) {
        generateCallExpr(*callExpr);
    } else if (auto indexExpr = dynamic_cast<const IndexExpr*>(&expr)) {
        generateIndexExpr(*indexExpr);
    } else if (auto historyExpr = dynamic_cast<const HistoryExpr*>(&expr)) {
        generateHistoryExpr(*historyExpr);
    } else if (auto pipelineExpr = dynamic_cast<const PipelineExpr*>(&expr)) {
        generatePipelineExpr(*pipelineExpr);
    } else if (auto arrayExpr = dynamic_cast<const ArrayExpr*>(&expr)) {
        generateArrayExpr(*arrayExpr);
    } else if (auto varExpr = dynamic_cast<const VariableExpr*>(&expr)) {
        generateVariableExpr(*varExpr);
    } else if (auto litExpr = dynamic_cast<const LiteralExpr*>(&expr)) {
        generateLiteralExpr(*litExpr);
    } else if (auto askExpr = dynamic_cast<const AskExpr*>(&expr)) {
        generateAskExpr(*askExpr);
    }
}

void CodeGen::generateBinaryExpr(const BinaryExpr& expr) {
    // Constant folding optimization
    Value leftVal, rightVal;
    if (isConstantExpr(*expr.left) && isConstantExpr(*expr.right) &&
        evaluateConstantExpr(*expr.left, leftVal) && evaluateConstantExpr(*expr.right, rightVal)) {
        // Evaluate at compile time
        double result = 0.0;
        if (expr.operation.type == TokenType::Plus) {
            if (leftVal.isNumber() && rightVal.isNumber()) {
                result = leftVal.asNumber() + rightVal.asNumber();
                emit(Instruction::PUSH_NUM, 0, 0, result);
                return;
            }
        } else if (expr.operation.type == TokenType::Minus) {
            if (leftVal.isNumber() && rightVal.isNumber()) {
                result = leftVal.asNumber() - rightVal.asNumber();
                emit(Instruction::PUSH_NUM, 0, 0, result);
                return;
            }
        } else if (expr.operation.type == TokenType::Star) {
            if (leftVal.isNumber() && rightVal.isNumber()) {
                result = leftVal.asNumber() * rightVal.asNumber();
                emit(Instruction::PUSH_NUM, 0, 0, result);
                return;
            }
        } else if (expr.operation.type == TokenType::Slash) {
            if (leftVal.isNumber() && rightVal.isNumber() && rightVal.asNumber() != 0) {
                result = leftVal.asNumber() / rightVal.asNumber();
                emit(Instruction::PUSH_NUM, 0, 0, result);
                return;
            }
        } else if (expr.operation.type == TokenType::Percent) {
            if (leftVal.isNumber() && rightVal.isNumber()) {
                result = static_cast<double>(static_cast<int>(leftVal.asNumber()) %
                                            static_cast<int>(rightVal.asNumber()));
                emit(Instruction::PUSH_NUM, 0, 0, result);
                return;
            }
        }
    }

    // Non-constant: generate normal code
    generateExpr(*expr.left);
    generateExpr(*expr.right);

    if (expr.operation.type == TokenType::Plus) {
        emit(Instruction::ADD);
    } else if (expr.operation.type == TokenType::Minus) {
        emit(Instruction::SUB);
    } else if (expr.operation.type == TokenType::Star) {
        emit(Instruction::MUL);
    } else if (expr.operation.type == TokenType::Slash) {
        emit(Instruction::DIV);
    } else if (expr.operation.type == TokenType::Percent) {
        emit(Instruction::MOD);
    } else if (expr.operation.type == TokenType::EqualEqual) {
        emit(Instruction::EQ);
    } else if (expr.operation.type == TokenType::BangEqual) {
        emit(Instruction::NE);
    } else if (expr.operation.type == TokenType::Less) {
        emit(Instruction::LT);
    } else if (expr.operation.type == TokenType::LessEqual) {
        emit(Instruction::LE);
    } else if (expr.operation.type == TokenType::Greater) {
        emit(Instruction::GT);
    } else if (expr.operation.type == TokenType::GreaterEqual) {
        emit(Instruction::GE);
    } else if (expr.operation.type == TokenType::And) {
        emit(Instruction::AND);
    } else if (expr.operation.type == TokenType::Or) {
        emit(Instruction::OR);
    }
}

void CodeGen::generateUnaryExpr(const UnaryExpr& expr) {
    generateExpr(*expr.right);

    if (expr.operation.type == TokenType::Minus) {
        emit(Instruction::NEG);
    } else if (expr.operation.type == TokenType::Not) {
        emit(Instruction::NOT);
    }
}

void CodeGen::generateCallExpr(const CallExpr& expr) {
    // Push arguments
    for (const auto& arg : expr.arguments) {
        generateExpr(*arg);
    }

    // Call function
    if (auto varExpr = dynamic_cast<const VariableExpr*>(expr.callee.get())) {
        uint32_t nameIdx = getNameIndex(varExpr->name.lexeme);
        emit(Instruction::CALL, nameIdx, static_cast<uint32_t>(expr.arguments.size()));
    }
}

void CodeGen::generateIndexExpr(const IndexExpr& expr) {
    generateExpr(*expr.object);
    generateExpr(*expr.index);
    emit(Instruction::INDEX);
}

void CodeGen::generateHistoryExpr(const HistoryExpr& expr) {
    if (auto varExpr = dynamic_cast<const VariableExpr*>(expr.object.get())) {
        uint32_t nameIdx = getNameIndex(varExpr->name.lexeme);
        emit(Instruction::LOAD_HIST, nameIdx, static_cast<uint32_t>(expr.assignmentsAgo));
    }
}

void CodeGen::generatePipelineExpr(const PipelineExpr& expr) {
    generateExpr(*expr.input);
    generateExpr(*expr.step);
    emit(Instruction::PIPELINE);
}

void CodeGen::generateArrayExpr(const ArrayExpr& expr) {
    for (const auto& elem : expr.elements) {
        generateExpr(*elem);
    }
    emit(Instruction::MAKE_ARRAY, static_cast<uint32_t>(expr.elements.size()));
}

void CodeGen::generateVariableExpr(const VariableExpr& expr) {
    uint32_t nameIdx = getNameIndex(expr.name.lexeme);
    emit(Instruction::LOAD, nameIdx);
}

void CodeGen::generateLiteralExpr(const LiteralExpr& expr) {
    if (expr.value.isNumber()) {
        double num = expr.value.asNumber();
        emit(Instruction::PUSH_NUM, 0, 0, num);
    } else if (expr.value.isString()) {
        std::string str = expr.value.toString();
        uint32_t strIdx = getStringIndex(str);
        emit(Instruction::PUSH_STR, strIdx);
    } else if (expr.value.isBool()) {
        if (expr.value.asBool()) {
            emit(Instruction::PUSH_TRUE);
        } else {
            emit(Instruction::PUSH_FALSE);
        }
    } else if (expr.value.isArray()) {
        // Empty array or array literal - handle in PUSH_ARRAY
        emit(Instruction::PUSH_ARRAY, 0);
    }
}

void CodeGen::generateAskExpr(const AskExpr& expr) {
    uint32_t promptIdx = getStringIndex(expr.prompt);
    emit(Instruction::ASK, promptIdx);
}

void CodeGen::generateExpressionStmt(const ExpressionStmt& stmt) {
    generateExpr(*stmt.expression);
    emit(Instruction::POP);
}

void CodeGen::generateAssignmentStmt(const AssignmentStmt& stmt) {
    generateExpr(*stmt.value);

    if (auto varExpr = dynamic_cast<const VariableExpr*>(stmt.target.get())) {
        uint32_t nameIdx = getNameIndex(varExpr->name.lexeme);
        if (stmt.operation.type == TokenType::FatArrow) {
            emit(Instruction::STORE, nameIdx);
        } else if (stmt.outer) {
            emit(Instruction::STORE_OUTER, nameIdx);
        } else {
            emit(Instruction::STORE, nameIdx);
        }
    } else if (auto indexExpr = dynamic_cast<const IndexExpr*>(stmt.target.get())) {
        // Handle indexed assignment: array[i] -> value
        if (auto varExpr2 = dynamic_cast<const VariableExpr*>(indexExpr->object.get())) {
            uint32_t nameIdx = getNameIndex(varExpr2->name.lexeme);
            generateExpr(*indexExpr->index);
            // Stack: [value, index], need to swap for STORE_INDEX
            emit(Instruction::STORE_INDEX, nameIdx);
        }
    }
}

void CodeGen::generateShowStmt(const ShowStmt& stmt) {
    generateExpr(*stmt.expression);
    emit(Instruction::SHOW);
}

void CodeGen::generateGiveStmt(const GiveStmt& stmt) {
    generateExpr(*stmt.expression);
    emit(Instruction::GIVE);
}

void CodeGen::generateRewindStmt(const RewindStmt& stmt) {
    uint32_t nameIdx = getNameIndex(stmt.name.lexeme);
    emit(Instruction::REWIND, nameIdx);
}

void CodeGen::generateBlockStmt(const BlockStmt& stmt) {
    for (const auto& s : stmt.statements) {
        generateStmt(*s);
    }
}

void CodeGen::generateWhenStmt(const WhenStmt& stmt) {
    std::vector<uint32_t> jumpAddresses;

    for (const auto& branch : stmt.branches) {
        generateExpr(*branch.condition);
        uint32_t falseJump = currentAddress();
        emit(Instruction::JUMP_IF_FALSE, 0);  // Patch later

        generateBlockStmt(*branch.body);
        jumpAddresses.push_back(currentAddress());
        emit(Instruction::JUMP, 0);  // Patch later

        patchJump(falseJump, currentAddress());
    }

    if (stmt.otherwiseBranch) {
        generateBlockStmt(*stmt.otherwiseBranch);
    }

    uint32_t endAddr = currentAddress();
    for (uint32_t addr : jumpAddresses) {
        patchJump(addr, endAddr);
    }
}

void CodeGen::generateRepeatStmt(const RepeatStmt& stmt) {
    generateExpr(*stmt.count);
    // TODO: Implement repeat loop bytecode
}

void CodeGen::generateUntilStmt(const UntilStmt& stmt) {
    uint32_t loopStart = currentAddress();
    generateBlockStmt(*stmt.body);
    generateExpr(*stmt.condition);
    emit(Instruction::JUMP_IF_FALSE, loopStart);
}

void CodeGen::generateEachStmt(const EachStmt& stmt) {
    uint32_t varIdx = getNameIndex(stmt.variable.lexeme);
    generateExpr(*stmt.collection);
    uint32_t loopAddr = currentAddress();
    emit(Instruction::ITER_EACH, varIdx, 0);  // Patch end address later
    generateBlockStmt(*stmt.body);
    emit(Instruction::JUMP, loopAddr);
    patchJump(loopAddr + 1, currentAddress());
}

void CodeGen::generateFunctionStmt(const FunctionStmt& stmt) {
    FunctionInfo info;
    info.name = stmt.name.lexeme;
    info.parameters = {};
    for (const auto& param : stmt.parameters) {
        info.parameters.push_back(param.lexeme);
    }
    info.codeStart = currentAddress();
    info.isBuiltin = false;

    generateBlockStmt(*stmt.body);
    emit(Instruction::PUSH_TRUE);  // Default return value
    emit(Instruction::RETURN);

    info.codeEnd = currentAddress();
    functions_.push_back(info);
}

bool CodeGen::isConstantExpr(const Expr& expr) {
    if (dynamic_cast<const LiteralExpr*>(&expr)) {
        return true;
    }
    if (auto binExpr = dynamic_cast<const BinaryExpr*>(&expr)) {
        return isConstantExpr(*binExpr->left) && isConstantExpr(*binExpr->right);
    }
    if (auto unaryExpr = dynamic_cast<const UnaryExpr*>(&expr)) {
        return isConstantExpr(*unaryExpr->right);
    }
    return false;
}

bool CodeGen::evaluateConstantExpr(const Expr& expr, Value& result) {
    if (auto litExpr = dynamic_cast<const LiteralExpr*>(&expr)) {
        result = litExpr->value;
        return true;
    }

    if (auto binExpr = dynamic_cast<const BinaryExpr*>(&expr)) {
        Value left, right;
        if (!evaluateConstantExpr(*binExpr->left, left) ||
            !evaluateConstantExpr(*binExpr->right, right)) {
            return false;
        }

        if (binExpr->operation.type == TokenType::Plus) {
            if (left.isNumber() && right.isNumber()) {
                result = Value(left.asNumber() + right.asNumber());
                return true;
            }
        } else if (binExpr->operation.type == TokenType::Minus) {
            if (left.isNumber() && right.isNumber()) {
                result = Value(left.asNumber() - right.asNumber());
                return true;
            }
        } else if (binExpr->operation.type == TokenType::Star) {
            if (left.isNumber() && right.isNumber()) {
                result = Value(left.asNumber() * right.asNumber());
                return true;
            }
        } else if (binExpr->operation.type == TokenType::Slash) {
            if (left.isNumber() && right.isNumber() && right.asNumber() != 0) {
                result = Value(left.asNumber() / right.asNumber());
                return true;
            }
        }
        return false;
    }

    if (auto unaryExpr = dynamic_cast<const UnaryExpr*>(&expr)) {
        Value operand;
        if (!evaluateConstantExpr(*unaryExpr->right, operand)) {
            return false;
        }

        if (unaryExpr->operation.type == TokenType::Minus) {
            if (operand.isNumber()) {
                result = Value(-operand.asNumber());
                return true;
            }
        } else if (unaryExpr->operation.type == TokenType::Not) {
            result = Value(!operand.isTruthy());
            return true;
        }
        return false;
    }

    return false;
}

void CodeGen::optimizeDeadCode() {
    // Remove code after GIVE/RETURN (simplified)
    std::vector<Instruction> optimized;
    for (const auto& instr : code_) {
        optimized.push_back(instr);
        if (instr.opcode == Instruction::GIVE || instr.opcode == Instruction::RETURN) {
            // Stop adding instructions after GIVE/RETURN
            // Note: This simplified version doesn't handle all cases correctly
            break;
        }
    }
    // In a real implementation, would be more sophisticated
}

void CodeGen::optimizeConstantFolding() {
    // Constant folding is handled at code generation time in generateBinaryExpr
}

}  // namespace flux
