#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "flux/CodeGen.h"
#include "flux/Interpreter.h"
#include "flux/Lexer.h"
#include "flux/Parser.h"
#include "flux/RuntimeError.h"
#include "flux/SemanticAnalyzer.h"
#include "flux/VM.h"

// Command-line entry point for the Flux front end. It reads one source file,
// reports lexical or syntax diagnostics, and executes the program or dumps
// compilation stages as requested.
int main(int argc, char* argv[]) {
    bool showMemory = false;
    bool showTokens = false;
    bool showAST = false;
    bool showBytecode = false;
    bool useBytecodeVM = false;
    std::string sourcePath;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mem") {
            showMemory = true;
        } else if (arg == "--tokens") {
            showTokens = true;
        } else if (arg == "--ast") {
            showAST = true;
        } else if (arg == "--bytecode") {
            showBytecode = true;
        } else if (arg == "--vm") {
            useBytecodeVM = true;
        } else if (arg[0] != '-') {
            sourcePath = arg;
        }
    }

    if (sourcePath.empty()) {
        std::cerr << "Usage: flux [--mem] [--tokens] [--ast] [--bytecode] [--vm] <program.flx>\n";
        return 64;
    }

    // Load the complete source file before starting the compiler pipeline.
    std::ifstream input(sourcePath);
    if (!input) {
        std::cerr << "Unable to open '" << sourcePath << "'.\n";
        return 66;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    input.close();

    // === STAGE 1: LEXICAL ANALYSIS ===
    flux::Lexer lexer(buffer.str());
    std::vector<flux::Token> tokens = lexer.scanTokens();
    if (!lexer.diagnostics().empty()) {
        for (const auto& diagnostic : lexer.diagnostics()) {
            std::cerr << diagnostic.toString() << '\n';
        }
        return 65;
    }

    if (showTokens) {
        std::cout << "[Tokens]\n";
        for (size_t i = 0; i < tokens.size(); ++i) {
            std::cout << i << ": " << tokens[i].lexeme << " (" << flux::tokenTypeName(tokens[i].type)
                      << ")\n";
        }
        std::cout << "[/Tokens]\n\n";
    }

    // === STAGE 2: SYNTAX ANALYSIS ===
    flux::Parser parser(std::move(tokens));
    flux::Program program = parser.parse();
    if (!parser.diagnostics().empty()) {
        for (const auto& diagnostic : parser.diagnostics()) {
            std::cerr << diagnostic.toString() << '\n';
        }
        return 65;
    }

    if (showAST) {
        std::cout << "[AST]\n";
        std::cout << "Program with " << program.statements.size() << " statements\n";
        for (size_t i = 0; i < program.statements.size(); ++i) {
            std::cout << "  Statement " << i << ": (node at " << program.statements[i].get() << ")\n";
        }
        std::cout << "[/AST]\n\n";
    }

    // === STAGE 3: SEMANTIC ANALYSIS ===
    flux::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    if (!analyzer.diagnostics().empty()) {
        for (const auto& diagnostic : analyzer.diagnostics()) {
            std::cerr << diagnostic.toString() << '\n';
        }
        return 65;
    }

    // === STAGE 4: CODE GENERATION & EXECUTION ===
    try {
        flux::Value::resetMemoryStats();

        if (useBytecodeVM) {
            // Bytecode VM execution path (experimental)
            flux::CodeGen codegen;
            std::vector<flux::Instruction> bytecode = codegen.generate(program);
            flux::VM vm(std::cin, std::cout, bytecode, codegen.constants(), showBytecode);
            vm.execute();
        } else {
            // Default: tree-walking interpreter
            flux::Interpreter interpreter(std::cin, std::cout);
            interpreter.interpret(program);
        }

        if (showMemory) {
            const flux::MemoryStats stats = flux::Value::memoryStats();
            std::cout << "[mem] arrays created: " << stats.arraysCreated
                      << ", peak live: " << stats.peakArraysLive << '\n';
        }
    } catch (const flux::RuntimeError& error) {
        std::cerr << "Runtime error, " << error.what() << '\n';
        return 70;
    }

    return 0;
}
