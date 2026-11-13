#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "flux/Lexer.h"
#include "flux/Interpreter.h"
#include "flux/Parser.h"
#include "flux/RuntimeError.h"
#include "flux/SemanticAnalyzer.h"

// Command-line entry point for the Flux front end. It reads one source file,
// reports lexical or syntax diagnostics, and confirms the parsed statement count.
int main(int argc, char* argv[]) {
    const bool showMemory = argc == 3 && std::string(argv[1]) == "--mem";
    if ((!showMemory && argc != 2) || (showMemory && argc != 3)) {
        std::cerr << "Usage: flux [--mem] <program.flx>\n";
        return 64;
    }

    const char* sourcePath = argv[showMemory ? 2 : 1];

    // Load the complete source file before starting the compiler pipeline.
    std::ifstream input(sourcePath);
    if (!input) {
        std::cerr << "Unable to open '" << sourcePath << "'.\n";
        return 66;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    // Stop after each compiler stage if that stage produced diagnostics.
    flux::Lexer lexer(buffer.str());
    std::vector<flux::Token> tokens = lexer.scanTokens();
    if (!lexer.diagnostics().empty()) {
        for (const auto& diagnostic : lexer.diagnostics()) {
            std::cerr << diagnostic.toString() << '\n';
        }
        return 65;
    }

    flux::Parser parser(std::move(tokens));
    flux::Program program = parser.parse();
    if (!parser.diagnostics().empty()) {
        for (const auto& diagnostic : parser.diagnostics()) {
            std::cerr << diagnostic.toString() << '\n';
        }
        return 65;
    }

    // Semantic analysis rejects structurally valid programs that violate Flux
    // rules before the interpreter is allowed to execute them.
    flux::SemanticAnalyzer analyzer;
    analyzer.analyze(program);
    if (!analyzer.diagnostics().empty()) {
        for (const auto& diagnostic : analyzer.diagnostics()) {
            std::cerr << diagnostic.toString() << '\n';
        }
        return 65;
    }

    // Execute only after all front-end stages have accepted the program.
    try {
        flux::Value::resetMemoryStats();
        flux::Interpreter interpreter(std::cin, std::cout);
        interpreter.interpret(program);
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
