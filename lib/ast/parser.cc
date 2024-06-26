/////////////////////////////////////////////////////////////
///                                                       ///
///     ██████╗ ██╗███████╗████████╗                      ///
///     ██╔══██╗██║██╔════╝╚══██╔══╝                      ///
///     ██████╔╝██║█████╗     ██║                         ///
///     ██╔══██╗██║██╔══╝     ██║                         ///
///     ██║  ██║██║██║        ██║                         ///
///     ╚═╝  ╚═╝╚═╝╚═╝        ╚═╝                         ///
///     * RIFT CORE - The official compiler for Rift.     ///
///     * Copyright (c) 2024, Rift-Org                    ///
///     * License terms may be found in the LICENSE file. ///
///                                                       ///
/////////////////////////////////////////////////////////////

#include <ast/parser.hh>
#include <ast/stmt.hh>
#include <error/error.hh>
#include <memory>
#include <ast/prgm.hh>
#include <ast/env.hh>
#include <utils/literals.hh>

using namespace rift::scanner;


namespace rift
{
    namespace ast
    {

        #pragma mark - Public API

        std::unique_ptr<Program> Parser::parse()
        {
            try {
                return program();
            } catch (const ParserException &e) {
                return nullptr;
            }
        }

        #pragma mark - Expressions Parsing

        std::unique_ptr<Expr> Parser::primary()
        {
            if (match({Token(TokenType::FALSE, "false", "", line)}))
                return std::unique_ptr<Literal>(new Literal(Token(TokenType::FALSE, "false", "", line)));
            if (match({Token(TokenType::TRUE, "true", "", line)}))
                return std::unique_ptr<Literal>(new Literal(Token(TokenType::TRUE, "true", "", line)));
            if (match({Token(TokenType::NIL, "nil", "", line)}))
                return std::unique_ptr<Literal>(new Literal(Token(TokenType::NIL, "nil", "", line)));

            if (match({Token(TokenType::NUMERICLITERAL, "", "", line)}))
                return std::unique_ptr<Literal>(new Literal(peekPrev(1)));
            if (match({Token(TokenType::STRINGLITERAL, "", "", line)}))
                return std::unique_ptr<Literal>(new Literal(Token(peekPrev(1))));
            if (match({Token(TokenType::IDENTIFIER, "", "", line)}))
                return std::unique_ptr<Literal>(new Literal(Token(peekPrev(1))));

            if (match({Token(TokenType::LEFT_PAREN, "(", "", line)})) { 
                auto expr = expression();
                consume(Token(TokenType::RIGHT_PAREN, ")", "", line), std::unique_ptr<ParserException>(new ParserException("Expected ')' after expression")));
                return expr;
            }

            return nullptr;
        }

        std::unique_ptr<Expr> Parser::unary()
        {
            if (match({Token(TokenType::BANG, "!", "", line)})) {
                auto op = peekPrev();
                auto right = unary();
                if (right == nullptr) rift::error::report(line, "unary", "Expected expression after unary operator", op, ParserException("Expected expression after unary operator"));
                return std::unique_ptr<Unary>(new Unary(op, std::move(right)));
            }

            if (match({Token(TokenType::MINUS, "-", "", line)})) {
                auto op = peekPrev();
                auto right = unary();
                if (right == nullptr) rift::error::report(line, "unary", "Expected expression after unary operator", op, ParserException("Expected expression after unary operator"));
                return std::unique_ptr<Unary>(new Unary(op, std::move(right)));
            }

            return primary();
        }

        std::unique_ptr<Expr> Parser::factor()
        {
            auto expr = unary();

            while (match({Token(TokenType::STAR, "*", "", line)}) || match({Token(TokenType::SLASH, "/", "", line)})) {
                auto op = peekPrev();
                auto right = unary();
                if (expr == nullptr) rift::error::report(line, "factor", "Expected number before factor operator", op, ParserException("Expected number before factor operator"));
                if (right == nullptr) rift::error::report(line, "factor", "Expected number after factor operator", op, ParserException("Expected number after factor operator"));
                expr = std::unique_ptr<Binary>(new Binary(std::move(expr), op, std::move(right)));
            }

            return expr;
        }

        std::unique_ptr<Expr> Parser::term()
        {
            auto expr = factor();

            while (match({Token(TokenType::MINUS, "-", "", line)}) || match({Token(TokenType::PLUS, "+", "", line)})) {
                auto op = peekPrev();
                auto right = factor();
                if (expr == nullptr) rift::error::report(line, "term", "Expected number before term operator", op, ParserException("Expected number before term operator"));
                if (right == nullptr) rift::error::report(line, "term", "Expected number after term operator", op, ParserException("Expected number after term operator"));
                expr = std::unique_ptr<Binary>(new Binary(std::move(expr), op, std::move(right)));
            }

            return expr;
        }

        std::unique_ptr<Expr> Parser::comparison()
        {
            auto expr = term();

            while (match({Token(TokenType::GREATER, ">", "", line)}) || match({Token(TokenType::GREATER_EQUAL, ">=", "", line)}) || match({Token(TokenType::LESS, "<", "", line)}) || match({Token(TokenType::LESS_EQUAL, "<=", "", line)}) ) {
                auto op = peekPrev();
                auto right = term();
                if (expr == nullptr) rift::error::report(line, "comparison", "Expected expression before comparison operator", op, ParserException("Expected expression before comparison operator"));
                if (right == nullptr) rift::error::report(line, "comparison", "Expected expression after comparison operator", op, ParserException("Expected expression after comparison operator"));
                expr = std::unique_ptr<Binary>(new Binary(std::move(expr), op, std::move(right)));
            }

            return expr;
        }

        std::unique_ptr<Expr> Parser::equality()
        {
            auto expr = comparison();

            while (match({Token(TokenType::BANG_EQUAL, "!=", "", line)}) || match({Token(TokenType::EQUAL_EQUAL, "==", "", line)})) {
                auto op = peekPrev();
                auto right = comparison();
                if (expr == nullptr) rift::error::report(line, "equality", "Expected expression before equality operator", op, ParserException("Expected expression before equality operator"));
                if (right == nullptr) rift::error::report(line, "equality", "Expected expression after equality operator", op, ParserException("Expected expression after equality operator"));
                expr = std::unique_ptr<Binary>(new Binary(std::move(expr), op, std::move(right)));
            }

            return expr;
        };

        std::unique_ptr<Expr> Parser::assignment()
        {
            if(peekNext() == Token(TokenType::EQUAL, "=", "", line)) {
                if (match({Token(TokenType::IDENTIFIER, "", "", line)})) {
                    if (peek(Token(TokenType::SEMICOLON, ";", "", line))) {
                        prevance();
                        return equality();
                    }
                    auto idt = peekPrev();
                    consume(Token(TokenType::EQUAL, "=", "", line), std::unique_ptr<ParserException>(new ParserException("Expected '=' after variable name")));
                    auto expr = assignment();
                    if (expr == nullptr) rift::error::report(line, "assignment", "Expected expression after variable name", peekPrev(), ParserException("Expected expression after variable name"));

                    // assignemnt operator expects lhs to be already declared
                    /// @note: this is just a check, the actual assignment is done in the evaluator
                    if (rift::ast::Environment::getInstance().getEnv(castString(idt)) == Token())
                        rift::error::report(line, "assignment", "🛑 Undefined variable '" + castString(idt) + "' at line: " + castNumberString(idt.line), idt, ParserException("Undefined variable '" + castString(idt) + "'"));

                    return std::unique_ptr<Assign>(new Assign(idt, std::move(expr)));
                }
            }

            return equality();
        }

        std::unique_ptr<Expr> Parser::expression()
        {
            return assignment();
        }

        #pragma mark - Statements Parsing

        std::unique_ptr<StmtExpr> Parser::statement_expression()
        {
            auto expr = expression();
            consume(Token(TokenType::SEMICOLON, ";", "", line), std::unique_ptr<ParserException>(new ParserException("Expected ';' after expression")));
            return std::unique_ptr<StmtExpr>(new StmtExpr(std::move(expr)));
        }

        std::unique_ptr<StmtPrint> Parser::statement_print()
        {
            consume(Token(TokenType::LEFT_PAREN, "(", "", line), std::unique_ptr<ParserException>(new ParserException("Expected '(' after print")));
            auto expr = expression();
            consume(Token(TokenType::RIGHT_PAREN, ")", "", line), std::unique_ptr<ParserException>(new ParserException("Expected ')' after print")));
            consume(Token(TokenType::SEMICOLON, ";", "", line), std::unique_ptr<ParserException>(new ParserException("Expected ';' after print statement")));
            return std::unique_ptr<StmtPrint>(new StmtPrint(expr));
        }

        #pragma mark - Declarations Parsing

        std::unique_ptr<DeclStmt> Parser::declaration_statement()
        {
            std::unique_ptr<Stmt> stmt;
            if (match_kw (Token(TokenType::PRINT, "", "", line))) {
                stmt = statement_print();
            } else {
                stmt = statement_expression();
            }

            return std::make_unique<DeclStmt>(std::move(stmt));
        }

        std::unique_ptr<DeclVar> Parser::declaration_variable()
        {
            // make sure there is an identifier
            auto idt = consume(Token(TokenType::IDENTIFIER, "", "", line), std::unique_ptr<ParserException>(new ParserException("Expected variable name")));
            prevance();
            // make sure the identifier is not already declared
            /// @note this is just a check, the actual declaration is done in the evaluator
            if (rift::ast::Environment::getInstance().getEnv(castString(idt)) != Token())
                rift::error::report(line, "declaration_variable", "🛑 Variable '" + castString(idt) + "' already declared at line: " + castNumberString(idt.line), idt, ParserException("Variable '" + castString(idt) + "' already declared"));

            auto expr = assignment();
            consume(Token(TokenType::SEMICOLON, ";", "", line), std::unique_ptr<ParserException>(new ParserException("Expected ';' after variable assignment")));
            return std::make_unique<DeclVar>(idt, std::move(expr));
        }

        #pragma mark - Program / Block Parsing

        std::unique_ptr<Block> Parser::block()
        {
            std::vector<std::unique_ptr<Decl>> decls = {};
            // rift::ast::Environment::addChild(); // create a new scope REMOVED (done in evaulator)

            while (!atEnd() && !peek(Token(TokenType::RIGHT_BRACE, "}", "", line))) {
                if (match({Token(TokenType::LEFT_BRACE, "{", "", line)})) {
                    auto inner_decls = std::move(block()->decls);
                    decls.insert(decls.end(), std::make_move_iterator(inner_decls->begin()), std::make_move_iterator(inner_decls->end()));
                } else if (match_kw (Token(TokenType::VAR, "", "", line))) {
                    decls.push_back(declaration_variable());
                } else {
                    decls.push_back(declaration_statement());
                }
            }

            if (!match({Token(TokenType::RIGHT_BRACE, "}", "", line)})) 
                rift::error::report(line, "statement_block", "Expected '}' after block", peek(), ParserException("Expected '}' after block"));
            // rift::ast::Environment::removeChild(); // remove the scope REMOVED (done in evaulator)

            std::unique_ptr<std::vector<std::unique_ptr<Decl>>> ret = std::make_unique<std::vector<std::unique_ptr<Decl>>>(decls);
            return std::make_unique<Block>(std::move(ret));
        }

        std::unique_ptr<Program> Parser::program()
        {
            vec_prog decls = std::make_unique<std::vector<std::unique_ptr<Decl>>>();

            while (!atEnd()) {
                if (match_kw (Token(TokenType::VAR, "", "", line))) {
                    decls->push_back(declaration_variable());
                } else if(match({Token(TokenType::LEFT_BRACE, "{", "", line)})) {
                    decls->push_back(block());
                } else {
                    decls->push_back(declaration_statement());
                }
            }

            return std::unique_ptr<Program>(new Program(std::move(decls)));
        }

        # pragma mark - Utilities

        void Parser::synchronize()
        {
            advance();

            while (!atEnd()) {
                if (peekPrev().type == TokenType::SEMICOLON) return;

                switch (peek().type) {
                    case TokenType::CLASS:
                    case TokenType::FUN:
                    case TokenType::VAR:
                    case TokenType::FOR:
                    case TokenType::IF:
                    case TokenType::WHILE:
                    case TokenType::PRINT:
                    case TokenType::RETURN:
                        return;
                    default:
                        break;
                }

                advance();
            }
        }
    }
}