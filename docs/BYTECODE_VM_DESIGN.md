# Flux Bytecode VM Architecture

## Overview

The Flux VM executes bytecode generated from the AST. This document defines the instruction set, memory model, and execution strategy.

## Execution Model

**Pipeline:** AST → CodeGen → Bytecode → VM execution

The VM is a **stack machine** with these components:
- **Instruction stream**: Vector of opcodes (32-bit words: opcode + operands)
- **Value stack**: Runtime values (numbers, strings, arrays, functions)
- **Call stack**: Function frames with local variables
- **Constant pool**: Static constants (strings, array literals)
- **Global scope**: Top-level variables and functions
- **Memory**: Reference-counted arrays and strings (existing system)

## Instruction Set (Opcodes)

| Opcode | Operands | Stack Effect | Purpose |
|--------|----------|--------------|---------|
| `HALT` | — | — | End program execution |
| `PUSH_NUM` | `double` | — → [num] | Push number constant |
| `PUSH_STR` | `index:u32` | — → [str] | Push string from constant pool (index into strings table) |
| `PUSH_TRUE` | — | — → [true] | Push boolean true |
| `PUSH_FALSE` | — | — → [false] | Push boolean false |
| `PUSH_NULL` | — | — → [null] | Push empty array `[]` |
| `LOAD` | `name:u32` | — → [value] | Load variable by name index |
| `LOAD_CONST` | `name:u32` | — → [value] | Load constant by name index (error if not found) |
| `STORE` | `name:u32` | [value] → — | Store to mutable variable, record history |
| `STORE_CONST` | `name:u32` | [value] → — | Define constant (error if already exists) |
| `STORE_OUTER` | `name:u32` | [value] → — | Store to variable in enclosing scope |
| `STORE_INDEX` | `name:u32` `depth:u8` | [array, index, value] → — | Store to array[index], record history |
| `ADD` | — | [a, b] → [result] | Add or concatenate |
| `SUB` | — | [a, b] → [result] | Subtract |
| `MUL` | — | [a, b] → [result] | Multiply or repeat string |
| `DIV` | — | [a, b] → [result] | Divide (error on divide-by-zero) |
| `MOD` | — | [a, b] → [result] | Modulo |
| `NEG` | — | [a] → [-a] | Negate |
| `NOT` | — | [bool] → [!bool] | Logical NOT |
| `EQ` | — | [a, b] → [a==b] | Equality |
| `NE` | — | [a, b] → [a!=b] | Inequality |
| `LT` | — | [a, b] → [a<b] | Less than |
| `LE` | — | [a, b] → [a<=b] | Less or equal |
| `GT` | — | [a, b] → [a>b] | Greater than |
| `GE` | — | [a, b] → [a>=b] | Greater or equal |
| `AND` | — | [a, b] → [a && b] | Logical AND (short-circuit: second operand already evaluated if taken) |
| `OR` | — | [a, b] → [a \|\| b] | Logical OR (short-circuit) |
| `MAKE_ARRAY` | `count:u32` | [v1, v2, ..., vN] → [array] | Create array from top N stack values |
| `INDEX` | — | [array, index] → [value] | Index into array, error if out of range |
| `LOAD_HIST` | `name:u32` `offset:u32` | — → [value] | Load from history (name~offset) |
| `HISTORY` | `name:u32` | — → [array] | Get full history of variable |
| `REWIND` | `name:u32` | — → — | Rewind variable to previous value |
| `POP` | — | [value] → — | Discard top of stack |
| `DUP` | — | [value] → [value, value] | Duplicate top of stack |
| `JUMP` | `addr:u32` | — → — | Jump to address |
| `JUMP_IF_FALSE` | `addr:u32` | [cond] → — | Conditional jump (pop condition) |
| `SKIP_IF_TRUE` | `addr:u32` | [cond] → — | Skip if true (pop condition) |
| `CALL` | `name:u32` `argc:u8` | [arg1, ..., argN] → [result] | Call function by name; pops N args, pushes result |
| `CALL_CLOSURE` | `argc:u8` | [fn, arg1, ..., argN] → [result] | Call closure on stack; pops N+1, pushes result |
| `RETURN` | — | [value] → — | Return from function with value |
| `SHOW` | — | [value] → — | Print value to stdout |
| `ASK` | `prompt_idx:u32` | — → [input] | Read from stdin with prompt |
| `ITER_START` | `collection_idx:u32` | [array] → — | Begin iteration (set up counter) |
| `ITER_NEXT` | `loop_idx:u32` `end_addr:u32` | — → [item] or jump | Fetch next iteration item or jump to end |
| `ITER_EACH` | `var_idx:u32` `collection_idx:u32` `end_addr:u32` | [array] → — | Setup `each` loop |
| `PUSH_ARRAY` | `count:u32` | [v1, ..., vN] → [array] | Push array literal |
| `DEFINE_FUNC` | `fn_idx:u32` `argc:u8` `body_addr:u32` | — → — | Define function (at compile time, not runtime) |
| `DEFINE_FN_CLOSURE` | `fn_idx:u32` `argc:u8` `capture_env:u32` `body_addr:u32` | — → [fn_value] | Define function as first-class value |
| `PUSH_BUILTIN` | `fn_name:u32` | — → [builtin_fn] | Push built-in function (len, push, history) |
| `PIPELINE` | — | [value, fn] → [result] | Apply fn to value |
| `BREAK` | — | — → — | Break from loop |
| `CONTINUE` | — | — → — | Continue to next iteration |
| `NEW_SCOPE` | — | — → — | Push a new scope frame |
| `END_SCOPE` | — | — → — | Pop scope frame |
| `GIVE` | — | [value] → — | Return from function (exception style) |

## Constant Pool

Immutable values shared across the program:
```
struct ConstantPool {
    std::vector<std::string> strings;  // String literals
    std::vector<std::string> names;    // Variable/function names
    std::vector<double> numbers;       // Number literals (optional, push directly)
}
```

Index mapping:
- `strings[i]` → `PUSH_STR i`
- `names[i]` → used in LOAD, STORE, CALL, etc.

## Call Stack Frame

Each function call creates a frame:
```
struct Frame {
    std::string functionName;
    Environment* localEnv;      // Local variable bindings
    uint32_t returnAddress;     // PC to resume after return
    std::vector<Value> savedStack;  // Stack snapshot (for yield/resume)
};
```

Locals are stored in `localEnv`, which chains to enclosing scopes for closures.

## Execution Loop (Pseudocode)

```cpp
class VM {
    std::vector<Instruction> code;
    std::vector<Value> stack;
    std::vector<Frame> callStack;
    ConstantPool constants;
    uint32_t pc;  // Program counter
    
    void execute() {
        while (pc < code.size()) {
            Instruction instr = code[pc++];
            switch (instr.opcode) {
                case PUSH_NUM:    stack.push(instr.operand1); break;
                case PUSH_STR:    stack.push(constants.strings[instr.operand1]); break;
                case ADD:         stack.push(stack.pop() + stack.pop()); break;
                case LOAD:        stack.push(environment.get(names[instr.operand1])); break;
                case STORE:       environment.set(names[instr.operand1], stack.pop()); break;
                case JUMP:        pc = instr.operand1; break;
                case JUMP_IF_FALSE:
                    if (!stack.pop().isTruthy()) pc = instr.operand1;
                    break;
                case CALL:
                    callFunction(constants.names[instr.operand1], instr.operand2);
                    break;
                case RETURN:
                    returnFromFunction();
                    break;
                case SHOW:
                    std::cout << stack.pop().toString() << '\n';
                    break;
                // ... etc for all opcodes
            }
        }
    }
};
```

## Optimization Opportunities

1. **Constant folding**: Compile-time evaluation of `2 + 3` → `5`
2. **Dead code elimination**: Remove unreachable code after `return`/`give`
3. **Jump optimization**: Remove redundant jumps
4. **Short-circuit evaluation**: `JUMP_IF_FALSE` for `&&` and `||`

## Memory Management

- Arrays and strings use existing reference-counted `Value` system
- Stack frame locals are `Value` objects (copied/moved as needed)
- Closure environments capture by reference (Environment chains)

## Error Handling

Runtime errors throw `RuntimeError`:
- Undefined variable → error
- Type mismatch (e.g., string + number) → error
- Array index out of range → error
- Division by zero → error
- Call depth limit exceeded → error
- Call stack underflow (malformed bytecode) → error

## Performance Notes

- **Stack-based**: No register allocation complexity
- **Linear bytecode**: Cache-friendly instruction stream
- **O(1) variable access**: Constant pool indices
- **O(log n) environment lookup**: Scope chain traversal (n = nesting depth)
