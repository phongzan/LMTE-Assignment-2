#include "flux/VM.h"

#include <algorithm>
#include <iostream>

namespace flux {

VM::VM(std::istream& input, std::ostream& output, const std::vector<Instruction>& code,
       const ConstantPool& constants, bool showBytecode)
    : code_(code),
      constants_(constants),
      showBytecode_(showBytecode),
      pc_(0),
      input_(input),
      output_(output),
      callDepth_(0) {
    globalEnv_ = std::make_unique<Environment>();
    envStack_.push_back(globalEnv_.get());

    if (showBytecode_) {
        output_ << "[Bytecode]\n";
        for (size_t i = 0; i < code_.size(); ++i) {
            output_ << i << ": ";
            switch (code_[i].opcode) {
                case Instruction::HALT:
                    output_ << "HALT\n";
                    break;
                case Instruction::PUSH_NUM:
                    output_ << "PUSH_NUM " << code_[i].numOperand << "\n";
                    break;
                case Instruction::PUSH_STR:
                    output_ << "PUSH_STR \"" << getStringFromIndex(code_[i].operand1) << "\"\n";
                    break;
                case Instruction::PUSH_TRUE:
                    output_ << "PUSH_TRUE\n";
                    break;
                case Instruction::PUSH_FALSE:
                    output_ << "PUSH_FALSE\n";
                    break;
                case Instruction::LOAD:
                    output_ << "LOAD " << getNameFromIndex(code_[i].operand1) << "\n";
                    break;
                case Instruction::STORE:
                    output_ << "STORE " << getNameFromIndex(code_[i].operand1) << "\n";
                    break;
                case Instruction::ADD:
                    output_ << "ADD\n";
                    break;
                case Instruction::SUB:
                    output_ << "SUB\n";
                    break;
                case Instruction::MUL:
                    output_ << "MUL\n";
                    break;
                case Instruction::DIV:
                    output_ << "DIV\n";
                    break;
                case Instruction::SHOW:
                    output_ << "SHOW\n";
                    break;
                default:
                    output_ << "OP(" << static_cast<int>(code_[i].opcode) << ")\n";
            }
        }
        output_ << "[/Bytecode]\n\n";
    }
}

void VM::execute() {
    while (pc_ < static_cast<uint32_t>(code_.size())) {
        const Instruction& instr = code_[pc_++];
        executeInstruction(instr);
    }
}

void VM::executeInstruction(const Instruction& instr) {
    switch (instr.opcode) {
        case Instruction::HALT:
            pc_ = code_.size();
            break;
        case Instruction::PUSH_NUM:
            push(Value(instr.numOperand));
            break;
        case Instruction::PUSH_STR:
            push(Value(getStringFromIndex(instr.operand1)));
            break;
        case Instruction::PUSH_TRUE:
            push(Value(true));
            break;
        case Instruction::PUSH_FALSE:
            push(Value(false));
            break;
        case Instruction::PUSH_ARRAY:
            push(Value::array(std::vector<Value>()));
            break;
        case Instruction::LOAD:
            handleLoad(instr.operand1);
            break;
        case Instruction::STORE:
            handleStore(instr.operand1);
            break;
        case Instruction::STORE_OUTER:
            handleStoreOuter(instr.operand1);
            break;
        case Instruction::ADD:
            handleAdd();
            break;
        case Instruction::SUB:
            handleSub();
            break;
        case Instruction::MUL:
            handleMul();
            break;
        case Instruction::DIV:
            handleDiv();
            break;
        case Instruction::MOD:
            handleMod();
            break;
        case Instruction::NEG:
            handleNeg();
            break;
        case Instruction::NOT:
            handleNot();
            break;
        case Instruction::EQ:
            handleEq();
            break;
        case Instruction::NE:
            handleNe();
            break;
        case Instruction::LT:
            handleLt();
            break;
        case Instruction::LE:
            handleLe();
            break;
        case Instruction::GT:
            handleGt();
            break;
        case Instruction::GE:
            handleGe();
            break;
        case Instruction::AND:
            handleAnd();
            break;
        case Instruction::OR:
            handleOr();
            break;
        case Instruction::INDEX:
            handleIndex();
            break;
        case Instruction::LOAD_HIST:
            handleLoadHist(instr.operand1, instr.operand2);
            break;
        case Instruction::HISTORY:
            handleHistory(instr.operand1);
            break;
        case Instruction::REWIND:
            handleRewind(instr.operand1);
            break;
        case Instruction::POP:
            pop();
            break;
        case Instruction::DUP: {
            Value v = peek();
            push(v);
            break;
        }
        case Instruction::JUMP:
            pc_ = instr.operand1;
            break;
        case Instruction::JUMP_IF_FALSE: {
            Value cond = pop();
            if (!cond.isTruthy()) {
                pc_ = instr.operand1;
            }
            break;
        }
        case Instruction::CALL:
            handleCall(instr.operand1, instr.operand2);
            break;
        case Instruction::SHOW:
            handleShow();
            break;
        case Instruction::ASK:
            handleAsk(instr.operand1);
            break;
        case Instruction::MAKE_ARRAY: {
            std::vector<Value> elements;
            for (uint32_t i = 0; i < instr.operand1; ++i) {
                elements.insert(elements.begin(), pop());
            }
            std::reverse(elements.begin(), elements.end());
            push(Value::array(elements));
            break;
        }
        case Instruction::GIVE:
            handleGive();
            break;
        case Instruction::RETURN:
            pc_ = code_.size();  // Simplified: jump to end
            break;
        default:
            throw RuntimeError("Unknown opcode: " + std::to_string(static_cast<int>(instr.opcode)));
    }
}

Value VM::pop() {
    if (stack_.empty()) {
        throw RuntimeError("Stack underflow");
    }
    Value v = stack_.back();
    stack_.pop_back();
    return v;
}

void VM::push(const Value& val) {
    stack_.push_back(val);
}

Value VM::peek(int offset) const {
    if (offset >= static_cast<int>(stack_.size())) {
        throw RuntimeError("Stack underflow");
    }
    return stack_[stack_.size() - 1 - offset];
}

void VM::handleLoad(uint32_t nameIdx) {
    std::string name = getNameFromIndex(nameIdx);
    Value val = envStack_.back()->get(name);
    push(val);
}

void VM::handleStore(uint32_t nameIdx) {
    std::string name = getNameFromIndex(nameIdx);
    Value val = pop();
    envStack_.back()->defineVariable(name, val);
}

void VM::handleStoreOuter(uint32_t nameIdx) {
    std::string name = getNameFromIndex(nameIdx);
    Value val = pop();
    if (envStack_.size() > 1) {
        // Store in parent scope
        envStack_[envStack_.size() - 2]->defineVariable(name, val);
    }
}

void VM::handleAdd() {
    Value b = pop();
    Value a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asNumber() + b.asNumber()));
    } else if (a.isString() || b.isString()) {
        push(Value(a.toString() + b.toString()));
    } else {
        throw RuntimeError("'+' needs two numbers or two strings");
    }
}

void VM::handleSub() {
    Value b = pop();
    Value a = pop();
    if (!a.isNumber() || !b.isNumber()) {
        throw RuntimeError("'-' needs two numbers");
    }
    push(Value(a.asNumber() - b.asNumber()));
}

void VM::handleMul() {
    Value b = pop();
    Value a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asNumber() * b.asNumber()));
    } else if ((a.isString() && b.isNumber()) || (a.isNumber() && b.isString())) {
        std::string str = a.isString() ? a.toString() : b.toString();
        int count = a.isNumber() ? static_cast<int>(a.asNumber()) : static_cast<int>(b.asNumber());
        std::string result;
        for (int i = 0; i < count; ++i) result += str;
        push(Value(result));
    } else {
        throw RuntimeError("'*' needs two numbers or two strings");
    }
}

void VM::handleDiv() {
    Value b = pop();
    Value a = pop();
    if (!a.isNumber() || !b.isNumber()) {
        throw RuntimeError("'/' needs two numbers");
    }
    double divisor = b.asNumber();
    if (divisor == 0) {
        throw RuntimeError("division by zero");
    }
    push(Value(a.asNumber() / divisor));
}

void VM::handleMod() {
    Value b = pop();
    Value a = pop();
    if (!a.isNumber() || !b.isNumber()) {
        throw RuntimeError("'%' needs two numbers");
    }
    push(Value(static_cast<double>(static_cast<int>(a.asNumber()) % static_cast<int>(b.asNumber()))));
}

void VM::handleNeg() {
    Value v = pop();
    if (!v.isNumber()) {
        throw RuntimeError("Negation requires a number");
    }
    push(Value(-v.asNumber()));
}

void VM::handleNot() {
    Value v = pop();
    push(Value(!v.isTruthy()));
}

void VM::handleEq() {
    Value b = pop();
    Value a = pop();
    push(Value(a.toString() == b.toString()));
}

void VM::handleNe() {
    Value b = pop();
    Value a = pop();
    push(Value(a.toString() != b.toString()));
}

void VM::handleLt() {
    Value b = pop();
    Value a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asNumber() < b.asNumber()));
    } else {
        push(Value(a.toString() < b.toString()));
    }
}

void VM::handleLe() {
    Value b = pop();
    Value a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asNumber() <= b.asNumber()));
    } else {
        push(Value(a.toString() <= b.toString()));
    }
}

void VM::handleGt() {
    Value b = pop();
    Value a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asNumber() > b.asNumber()));
    } else {
        push(Value(a.toString() > b.toString()));
    }
}

void VM::handleGe() {
    Value b = pop();
    Value a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asNumber() >= b.asNumber()));
    } else {
        push(Value(a.toString() >= b.toString()));
    }
}

void VM::handleAnd() {
    Value b = pop();
    Value a = pop();
    push(Value(a.isTruthy() && b.isTruthy()));
}

void VM::handleOr() {
    Value b = pop();
    Value a = pop();
    push(Value(a.isTruthy() || b.isTruthy()));
}

void VM::handleIndex() {
    Value idx = pop();
    Value arr = pop();
    if (!arr.isArray() || !idx.isNumber()) {
        throw RuntimeError("Invalid index operation");
    }
    int index = static_cast<int>(idx.asNumber());
    const auto& array = arr.asArray();
    if (index < 0 || index >= static_cast<int>(array.size())) {
        throw RuntimeError("index " + std::to_string(index) + " out of range for array of length " +
                           std::to_string(array.size()));
    }
    push(array[index]);
}

void VM::handleLoadHist(uint32_t nameIdx, uint32_t offset) {
    std::string name = getNameFromIndex(nameIdx);
    // TODO: Implement history loading
    push(Value(0.0));
}

void VM::handleHistory(uint32_t nameIdx) {
    std::string name = getNameFromIndex(nameIdx);
    // TODO: Implement history collection
    push(Value::array(std::vector<Value>()));
}

void VM::handleRewind(uint32_t nameIdx) {
    std::string name = getNameFromIndex(nameIdx);
    // TODO: Implement rewind
}

void VM::handleShow() {
    Value v = pop();
    output_ << v.toString() << '\n';
}

void VM::handleAsk(uint32_t promptIdx) {
    std::string prompt = getStringFromIndex(promptIdx);
    output_ << prompt;
    std::string line;
    if (std::getline(input_, line)) {
        // Try to parse as number
        try {
            size_t idx;
            double num = std::stod(line, &idx);
            if (idx == line.size()) {
                push(Value(num));
                return;
            }
        } catch (...) {
        }
        // Otherwise, treat as string
        push(Value(line));
    }
}

void VM::handleCall(uint32_t nameIdx, uint32_t argc) {
    std::string name = getNameFromIndex(nameIdx);

    std::vector<Value> args;
    for (uint32_t i = 0; i < argc; ++i) {
        args.insert(args.begin(), pop());
    }

    // TODO: Call user-defined functions
    // For now, try to call built-in functions
    Value result = callBuiltin(name, args);
    push(result);
}

void VM::handleGive() {
    // Simplified: treat like return
    pop();
}

Value VM::callBuiltin(const std::string& name, const std::vector<Value>& args) {
    if (name == "len") {
        return builtinLen(args);
    } else if (name == "push") {
        return builtinPush(args);
    } else if (name == "history") {
        return builtinHistory(args);
    }
    throw RuntimeError("undefined function '" + name + "'");
}

Value VM::builtinLen(const std::vector<Value>& args) {
    if (args.size() != 1 || !args[0].isArray()) {
        throw RuntimeError("len() requires an array");
    }
    return Value(static_cast<double>(args[0].asArray().size()));
}

Value VM::builtinPush(const std::vector<Value>& args) {
    if (args.size() != 2 || !args[0].isArray()) {
        throw RuntimeError("push() requires an array and a value");
    }
    const auto& array = args[0].asArray();
    std::vector<Value> newArray = array;
    newArray.push_back(args[1]);
    return Value::array(newArray);
}

Value VM::builtinHistory(const std::vector<Value>& args) {
    // TODO: Implement history collection from environment
    return Value::array(std::vector<Value>());
}

}  // namespace flux
