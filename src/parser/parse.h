#include <types.h>
#include <lexer/token.h>
#include <vector>
#include <stack>

namespace gjs {
    class vm_context;
    enum class error_code;

    namespace parse {
        struct ast;
        struct context;

        ast* identifier(context& ctx);

        ast* type_identifier(context& ctx);

        ast* expression(context& ctx);

        ast* variable_declaration(context& ctx);

        ast* class_declaration(context& ctx);

        ast* function_declaration(context& ctx);

        ast* format_declaration(context& ctx);

        ast* argument_list(context& ctx);

        ast* if_statement(context& ctx);

        ast* return_statement(context& ctx);

        ast* delete_statement(context& ctx);

        ast* while_loop(context& ctx);

        ast* for_loop(context& ctx);

        ast* do_while_loop(context& ctx);

        ast* block(context& ctx);

        ast* single(context& ctx);

        ast* parse(vm_context* env, const std::string& file, const std::vector<lex::token>& tokens);
    };
};