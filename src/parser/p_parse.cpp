#include <gjs/parser/parse.h>
#include <gjs/parser/context.h>
#include <gjs/parser/ast.h>
#include <gjs/common/errors.h>

namespace gjs {
    using tt = lex::token_type;
    using exc = error::exception;
    using ec = error::ecode;
    using token = lex::token;

    namespace parse {
        using nt = ast::node_type;

        ast* identifier(context& ctx) {
            const token& t = ctx.current();
            if (t.type != tt::identifier) {
                throw exc(ec::p_expected_identifier, t.src);
            }
            ctx.consume();

            ast* n = new ast();
            n->type = nt::identifier;
            n->src(t);
            n->set(t.text);
            return n;
        }

        ast* type_identifier(context& ctx) {
            const token& t = ctx.current();
            if (t.type != tt::identifier) {
                throw exc(ec::p_expected_type_identifier, t.src);
            }
            ctx.consume();

            ast* n = new ast();
            n->type = nt::type_identifier;
            n->src(t);
            n->set(t.text);

            if (ctx.match({ "." }) && !ctx.is_typename(t.text)) {
                ctx.consume();
                n->rvalue = identifier(ctx);
                std::string tn = *n->rvalue;

                bool is_type = false;
                for (u16 m = 0;m < ctx.named_imports.size() && !is_type;m++) {
                    if (ctx.named_imports[m].first == t.text) {
                        for (u16 i = 0;i < ctx.named_imports[m].second.size() && !is_type;i++) {
                            is_type = ctx.named_imports[m].second[i] == tn;
                        }
                    }
                }

                if (!is_type) {
                    throw exc(ec::p_expected_type_identifier, t.src);
                }
            } else if (!ctx.is_typename(t.text)) throw exc(ec::p_expected_type_identifier, t.src);

            if (ctx.match({ "<" })) {
                ctx.consume();

                // subtype
                n->data_type = type_identifier(ctx);

                if (!ctx.match({ ">" })) {
                    throw exc(ec::p_expected_char, ctx.current().src, '>');
                }
                ctx.consume();
            }

            return n;
        }

        ast* variable_declaration(context& ctx) {
            ast* r = new ast();
            r->type = nt::variable_declaration;
            r->data_type = type_identifier(ctx);
            r->ref = r->data_type->ref;
            r->identifier = identifier(ctx);

            if (ctx.match({ "=" })) {
                ast* i = new ast();
                i->type = nt::variable_initializer;
                i->src(ctx.current());
                ctx.consume();
                i->body = expression(ctx);
                r->initializer = i;
            }

            return r;
        }

        ast* block(context& ctx) {
            if (!ctx.match({ tt::open_block })) {
                throw exc(ec::p_expected_char, ctx.current().src, '{');
            }
            ctx.consume();

            ast* r = new ast();
            r->type = nt::scoped_block;
            r->src(ctx.current());

            ctx.path.push(r);

            ast* node = nullptr;
            do {
                node = any(ctx);
                if (node) {
                    if (r->body) {
                        ast* n = r->body;
                        while (n->next) n = n->next;
                        n->next = node;
                    } else r->body = node;
                }
            } while (node != nullptr);

            ctx.path.pop();

            if (!ctx.match({ tt::close_block })) {
                throw exc(ec::p_expected_char, ctx.current().src, '}');
            }
            ctx.consume();

            return r;
        }

        ast* any(context& ctx) {
            if (ctx.match({ tt::eof })) {
                ctx.consume();
                return nullptr;
            }

            if (ctx.match({ tt::semicolon })) {
                ast* r = new ast();
                r->type = nt::empty;
                r->src(ctx.current());
                ctx.consume();
                return r;
            }


            ast* r = nullptr;

            if (ctx.match({ tt::keyword })) {
                if (ctx.match({ "const" })) {
                    r = variable_declaration(ctx);
                    r->is_const = true;
                }
                else if (ctx.match({ "static" })) {
                    r = variable_declaration(ctx);
                    r->is_static = true;
                }
                else if (ctx.match({ "if" })) r = if_statement(ctx);
                else if (ctx.match({ "for" })) r = for_loop(ctx);
                else if (ctx.match({ "while" })) r = while_loop(ctx);
                else if (ctx.match({ "do" })) r = do_while_loop(ctx);
                else if (ctx.match({ "format" })) r = format_declaration(ctx);
                else if (ctx.match({ "class" })) r = class_declaration(ctx);
                else if (ctx.match({ "this", "null", "new" })) r = expression(ctx);
                else if (ctx.match({ "delete" })) r = delete_statement(ctx);
                else if (ctx.match({ "return" })) r = return_statement(ctx);
                else if (ctx.match({ "import" })) r = import_statement(ctx);
                else if (ctx.match({ "enum" })) r = enum_declaration(ctx);
                else {
                    throw exc(ec::p_unexpected_token, ctx.current().src, ctx.current().text.c_str());
                }

                bool semicolon_optional = false;
                if (r->type == nt::function_declaration) semicolon_optional = true;
                else if (r->type == nt::format_declaration) semicolon_optional = true;
                else if (r->type == nt::class_declaration) semicolon_optional = true;
                else if (r->type == nt::if_statement) semicolon_optional = true;
                else if (r->type == nt::for_loop) semicolon_optional = true;
                else if (r->type == nt::while_loop) semicolon_optional = true;
                else if (r->type == nt::do_while_loop) semicolon_optional = true;
                else if (r->type == nt::enum_declaration) semicolon_optional = true;
                else if (r->type == nt::empty) semicolon_optional = true;

                if (!semicolon_optional) {
                    if (!ctx.match({ tt::semicolon })) throw exc(ec::p_expected_char, ctx.current().src, ';');
                    ctx.consume();
                } else if (ctx.match({ tt::semicolon })) {
                    ctx.consume();
                }

                return r;
            }


            token t = ctx.current();
            if (ctx.is_typename()) {
                // one of three things:
                // - Expression starting with static class property or method
                // - Function declaration and/or definition
                // - Variable declaration
                ctx.backup();
                ast* dummy = type_identifier(ctx);
                delete dummy;

                if (ctx.match({ tt::member_accessor })) {
                    ctx.restore();
                    r = expression(ctx);
                } else {
                    if (ctx.pattern({ tt::identifier, tt::open_parenth })) {
                        ctx.restore();
                        r = function_declaration(ctx);
                    } else {
                        ctx.restore();
                        r = variable_declaration(ctx);
                    }
                }

                bool semicolon_optional = false;
                if (r->type == nt::function_declaration) semicolon_optional = true;
                if (r->type == nt::empty) semicolon_optional = true;

                if (!semicolon_optional) {
                    if (!ctx.match({ tt::semicolon })) throw exc(ec::p_expected_char, ctx.current().src, ';');
                    ctx.consume();
                } else if (ctx.match({ tt::semicolon })) {
                    ctx.consume();
                }

                return r;
            }

            if (t == tt::identifier) {
                r = expression(ctx);
                if (!ctx.match({ tt::semicolon })) throw exc(ec::p_expected_char, ctx.current().src, ';');
                ctx.consume();
                return r;
            }

            if (t == tt::open_parenth) {
                r = expression(ctx);
                if (!ctx.match({ tt::semicolon })) throw exc(ec::p_expected_char, ctx.current().src, ';');
                ctx.consume();
                return r;
            }
            
            if (t == tt::open_block) {
                return block(ctx);
            }

            return r;
        }

        ast* parse(script_context* env, const std::string& module, const std::vector<lex::token>& tokens) {
            context ctx(env);
            ctx.tokens = tokens;

            ctx.root = new ast();
            ctx.root->type = nt::root;
            ctx.root->ref.module = module;
            ctx.path.push(ctx.root);

            ast* node = nullptr;
            do {
                node = any(ctx);
                if (node) {
                    if (ctx.root->body) {
                        ast* n = ctx.root->body;
                        while (n->next) n = n->next;
                        n->next = node;
                    } else ctx.root->body = node;
                }
            } while (node != nullptr && !ctx.at_end());

            return ctx.root;
        }
    };
};