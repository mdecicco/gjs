#include <gjs/parser/parse_utils.h>
#include <stdarg.h>
using namespace std;

namespace gjs {
    parse_exception::parse_exception(const std::string& _module, const string& _text, const string& _lineText, u32 _line, u32 _col) {
        module = _module;
        text = _text;
        lineText = _lineText;
        line = _line;
        col = _col;
    }

    parse_exception::~parse_exception() { }

    tokenizer::tokenizer(const string& input) {
        m_input = input;
        m_line = 0;
        m_col = 0;
        m_idx = 0;

        lines = get_lines(input);
    }

    tokenizer::~tokenizer() {
    }

    void tokenizer::specify_keyword(const string& keyword) {
        m_keywords.push_back(keyword);
    }

    void tokenizer::specify_operator(const std::string& op) {
        m_operators.push_back(op);
        m_keywords.push_back(op);
    }

    bool tokenizer::is_operator(const std::string& thing) {
        if (thing.length() == 0) return false;

        for (const string& op : m_operators) {
            if (op == thing) return true;
        }

        return false;
    }
    
    bool tokenizer::is_keyword(const string& thing) {
        if (thing.length() == 0) return false;

        for (const string& kw : m_keywords) {
            if (kw == thing) return true;
        }

        return false;
    }

    bool tokenizer::is_number(const string& thing) {
        if (thing.length() == 0) return false;
        bool decimal_found = false;
        for (u32 idx = 0;idx < thing.length();idx++) {
            if (thing[idx] == '-' && idx == 0) continue;
            if (thing[idx] == '.') {
                if (decimal_found) return false;
                decimal_found = true;
                continue;
            }
            if (thing[idx] == 'f' && idx == thing.length() - 1) continue;
            if (thing[idx] < 48 || thing[idx] > 57) return false;
        }

        return true;
    }

    bool tokenizer::is_identifier(const string& thing) {
        if (thing.length() == 0) return false;
        if ((thing[0] < 65 || thing[0] > 90) && (thing[0] < 97 || thing[0] > 122) && thing[0] != '_') return false; 
        for (u32 idx = 0;idx < thing.length();idx++) {
            char c = thing[idx];
            if (
               (c >= 48 && c <= 57 ) // 0 - 9
            || (c >= 65 && c <= 90 ) // A - Z
            || (c >= 97 && c <= 122) // a - z
            || c == '_'
            ) continue;
            return false;
        }

        return !is_keyword(thing);
    }

    void tokenizer::backup_state() {
        m_storedState.push({ m_idx, m_line, m_col });
    }

    void tokenizer::commit_state() {
        m_storedState.pop();
    }

    void tokenizer::restore_state() {
        auto s = m_storedState.top();
        m_storedState.pop();
        m_idx = s.idx;
        m_line = s.line;
        m_col = s.col;
    }

    bool tokenizer::whitespace() {
        char last = 0;
        bool hadWhitespace = false;
        while (!at_end(false)) {
            char c = m_input[m_idx];
            if (c == ' ' || c == '\t') {
                hadWhitespace = true;
                last = c;
                m_col++;
                m_idx++;
                continue;
            }

            if (newline()) {
                hadWhitespace = true;
                continue;
            }

            break;
        }

        return hadWhitespace;
    }

    bool tokenizer::newline() {
        if (m_input[m_idx] == '\n') {
            m_idx++;
            m_line++;
            m_col = 0;
            if (m_idx < m_input.size() && m_input[m_idx] == '\r') m_idx++;
            return true;
        }

        if (m_input[m_idx] == '\r') {
            m_idx++;
            m_line++;
            m_col = 0;
            if (m_idx < m_input.size() && m_input[m_idx] == '\n') m_idx++;
            return true;
        }

        return false;
    }

    string tokenizer::thing() {
        string out;
        while(!at_end()) {
            if (whitespace()) break;
            out += m_input[m_idx++];
        }

        return out;
    }

    tokenizer::token tokenizer::any_token() {
        token t;
        t = identifier(false);
        if (t) return t;
        t = string_constant(false);
        if (t) return t;
        t = number_constant(false);
        if (t) return t;
        t = keyword(false);
        if (t) return t;

        // fallback

        whitespace();
        token out;
        out.line = m_line;
        out.col = m_col;
        out.module = module;
        bool is_alnum = false;
        bool is_num = false;
        bool found_decimal = false;
        bool is_op = false;
        if (at_end()) return out;

        char c = m_input[m_idx];
        if (isalpha(c)) is_alnum = true;
        else if (c >= 48 && c <= 57) is_num = true;
        else if (c == '-' && m_input[m_idx + 1ull] >= 48 && m_input[m_idx + 1ull] <= 57) {
            out.text += m_input[m_idx++];
            is_num = true;
        }


        while(!at_end(false)) {
            if (whitespace()) break;
            c = m_input[m_idx];
            if (out.text.length() > 0 && (c == '(' || c == ')')) break;
            if (is_alnum && !isalnum(c)) break;
            else if (is_num && (!(c >= 48 && c <= 57) && c != '.')) break;
            else if (!is_alnum && !is_num && isalnum(c)) break;
            if (c == '.' && is_num) {
                if (found_decimal) {
                    throw parse_exception(
                        module,
                        "Invalid numerical constant",
                        lines[m_line],
                        m_line,
                        m_col
                    );
                } else found_decimal = true;
            }
            out.text += c;
            m_idx++;
            if (c == '(' || c == ')') break;
        }

        return out;
    }

    tokenizer::token tokenizer::semicolon(bool expected) {
        return character(';', expected);
    }

    tokenizer::token tokenizer::keyword(bool expected, const string& kw) {
        whitespace();

        backup_state();
        token ident = identifier(false);
        restore_state();
        if (ident) {
            if (expected) {
                if (kw.length() > 0) {
                    throw parse_exception(
                        module,
                        format("Expected '%s'", kw.c_str()),
                        lines[ident.line],
                        ident.line,
                        ident.col
                    );
                } else {
                    throw parse_exception(
                        module,
                        "Expected keyword",
                        lines[ident.line],
                        ident.line,
                        ident.col
                    );
                }
            }

            return token();
        }

        token out = { m_line, m_col, "", module };

        u64 offset = 0;

        if (kw.length() == 0) {
            u64 max_len = 0;
            for (u32 i = 0;i < m_keywords.size();i++) {
                if (m_keywords[i].length() > max_len) max_len = m_keywords[i].length();
            }
            token longest_kw;
            u64 kw_offset = 0;
            while((offset + m_idx) < m_input.size() && out.text.length() < max_len) {
                char c = m_input[m_idx + offset];
                offset++;
                out.text += c;
                if (is_keyword(out.text) && (m_input.size() >= m_idx + offset || !is_keyword(out.text + m_input[m_idx + offset]))) {
                    longest_kw = out;
                    kw_offset = offset;
                }
            }

            if (!longest_kw && expected) {
                if (!expected) return token();
                throw parse_exception(
                    module,
                    "Expected keyword",
                    lines[m_line],
                    m_line,
                    m_col
                );
            } else if (!longest_kw) return token();

            m_idx += kw_offset;
            m_col += (u32)kw_offset;
            return longest_kw;
        }

        while(!at_end()) {
            char c = m_input[m_idx + offset];
            if (c >= 48 && c <= 57 && out.text.length() == 0) {
                if (!expected) return out;
                // 0 - 9
                throw parse_exception(
                    module,
                    "Expected keyword, found numerical constant",
                    lines[m_line],
                    m_line,
                    m_col
                );
            }

            if (c == '\'' || c == '"' && out.text.length() == 0) {
                if (!expected) return out;
                throw parse_exception(
                    module,
                    "Expected keyword, found string constant",
                    lines[m_line],
                    m_line,
                    m_col
                );
            }

            if (
                (c >= 48 && c <= 57 ) || // 0 - 9
                (c >= 65 && c <= 90 ) || // A - Z
                (c >= 97 && c <= 122) || // a - z
                c == '_' || c == '+' || c == '-' ||
                c == '*' || c == '/' || c == '&' ||
                c == '%' || c == '^' || c == '|' ||
                c == '=' || c == '!' || c == '<' ||
                c == '>'
            ) {
                out.text += c;
                offset++;
            } else {
                if (out.text.length() == 0) {
                    if (!expected) return out;
                    throw parse_exception(
                        module,
                        "Expected keyword, found something else",
                        lines[m_line],
                        m_line,
                        m_col
                    );
                } else break;
            }
        }

        if (!is_keyword(out.text)) {
            if (!expected) return token();
            throw parse_exception(
                module,
                "Expected keyword, found identifier",
                lines[m_line],
                m_line,
                m_col
            );
        }

        if (out.text != kw && kw.length() > 0) {
            if (!expected) return token();
            throw parse_exception(
                module,
                format("Expected keyword '%s', found '%s'", kw.c_str(), out.text.c_str()),
                lines[m_line],
                m_line,
                m_col
            );
        }

        m_idx += offset;
        m_col += (u32)offset;

        return out;
    }

    tokenizer::token tokenizer::operat0r(bool expected, const string& op) {
        whitespace();

        backup_state();
        token ident = identifier(false);
        restore_state();
        if (ident) {
            if (expected) {
                if (op.length() > 0) {
                    throw parse_exception(
                        module,
                        format("Expected '%s'", op.c_str()),
                        lines[ident.line],
                        ident.line,
                        ident.col
                    );
                } else {
                    throw parse_exception(
                        module,
                        "Expected operator",
                        lines[ident.line],
                        ident.line,
                        ident.col
                    );
                }
            }

            return token();
        }

        token out = { m_line, m_col, "", module };

        size_t offset = 0;

        if (op.length() == 0) {
            size_t max_len = 0;
            for (u32 i = 0;i < m_operators.size();i++) {
                if (m_operators[i].length() > max_len) max_len = m_operators[i].length();
            }
            token longest_kw;
            size_t kw_offset = 0;
            while((offset + m_idx) < m_input.size() && out.text.length() < max_len) {
                char c = m_input[m_idx + offset];
                offset++;
                out.text += c;
                if (is_operator(out.text) && (m_input.size() >= m_idx + offset || !is_operator(out.text + m_input[m_idx + offset]))) {
                    longest_kw = out;
                    kw_offset = offset;
                }
            }

            if (!longest_kw && expected) {
                if (!expected) return token();
                throw parse_exception(
                    module,
                    "Expected operator",
                    lines[m_line],
                    m_line,
                    m_col
                );
            } else if (!longest_kw) return token();

            m_idx += kw_offset;
            m_col += (u32)kw_offset;
            return longest_kw;
        }

        while(!at_end()) {
            char c = m_input[m_idx + offset];
            if (c >= 48 && c <= 57 && out.text.length() == 0) {
                if (!expected) return out;
                // 0 - 9
                throw parse_exception(
                    module,
                    "Expected operator, found numerical constant",
                    lines[m_line],
                    m_line,
                    m_col
                );
            }

            if (c == '\'' || c == '"' && out.text.length() == 0) {
                if (!expected) return out;
                throw parse_exception(
                    module,
                    "Expected operator, found string constant",
                    lines[m_line],
                    m_line,
                    m_col
                );
            }

            if (
                (c >= 48 && c <= 57 ) || // 0 - 9
                (c >= 65 && c <= 90 ) || // A - Z
                (c >= 97 && c <= 122) || // a - z
                c == '_' || c == '+' || c == '-' ||
                c == '*' || c == '/' || c == '&' ||
                c == '%' || c == '^' || c == '|' ||
                c == '=' || c == '!' || c == '<' ||
                c == '>'
                ) {
                out.text += c;
                offset++;
            } else {
                if (out.text.length() == 0) {
                    if (!expected) return out;
                    throw parse_exception(
                        module,
                        "Expected operator, found something else",
                        lines[m_line],
                        m_line,
                        m_col
                    );
                } else break;
            }
        }

        if (!is_operator(out.text)) {
            if (!expected) return token();
            throw parse_exception(
                module,
                "Expected operator, found identifier",
                lines[m_line],
                m_line,
                m_col
            );
        }

        if (out.text != op && op.length() > 0) {
            if (!expected) return token();
            throw parse_exception(
                module,
                format("Expected operator '%s', found '%s'", op.c_str(), out.text.c_str()),
                lines[m_line],
                m_line,
                m_col
            );
        }

        m_idx += offset;
        m_col += (u32)offset;

        return out;
    }

    tokenizer::token tokenizer::identifier(bool expected, const string& identifier) {
        whitespace();
        token out = { m_line, m_col, "", module };

        size_t offset = 0;

        while(!at_end()) {
            char c = m_input[m_idx + offset];
            if (c >= 48 && c <= 57 && out.text.length() == 0) {
                if (!expected) return out;
                // 0 - 9
                throw parse_exception(
                    module,
                    "Expected identifier, found numerical constant",
                    lines[m_line],
                    m_line,
                    m_col
                );
            }

            if (c == '\'' || c == '"' && out.text.length() == 0) {
                if (!expected) return out;
                throw parse_exception(
                    module,
                    "Expected identifier, found string constant",
                    lines[m_line],
                    m_line,
                    m_col
                );
            }

            if (
                (c >= 48 && c <= 57 ) || // 0 - 9
                (c >= 65 && c <= 90 ) || // A - Z
                (c >= 97 && c <= 122) || // a - z
                c == '_'
            ) {
                out.text += c;
                offset++;
            } else {
                if (out.text.length() == 0) {
                    if (!expected) return out;
                    throw parse_exception(
                        module,
                        "Expected identifier, found something else",
                        lines[m_line],
                        m_line,
                        m_col
                    );
                } else break;
            }
        }
        
        if (is_keyword(out.text)) {
            if (!expected) return token();
            throw parse_exception(
                module,
                "Expected identifier, found keyword",
                lines[m_line],
                m_line,
                m_col
            );
        }

        if (out.text != identifier && identifier.length() > 0) {
            if (!expected) return token();
            throw parse_exception(
                module,
                format("Expected identifier '%s', found '%s'", identifier.c_str(), out.text.c_str()),
                lines[m_line],
                m_line,
                m_col
            );
        }

        m_idx += offset;
        m_col += (u32)offset;

        return out;
    }

    tokenizer::token tokenizer::character(char c, bool expected) {
        whitespace();
        token out = { m_line, m_col, "", module };

        if (m_input[m_idx] == c) {
            out.text = c;
            m_idx++;
            m_col++;
        } else {
            if (!expected) return out;
            string found = thing();
            throw parse_exception(
                module,
                format("Expected '%c', found \"%s\"", c, found.c_str()),
                lines[m_line],
                m_line,
                m_col
            );
        }

        return out;
    }

    tokenizer::token tokenizer::expression(bool expected) {
        if (!expected) backup_state();
        whitespace();

        u8 p_level = 0;
        std::stack<token> parens;
        token out = { m_line, m_col, "", module };
        token quote;
        bool lastWasNewline = false;

        char last = 0;
        while (!at_end(false)) {
            char c = m_input[m_idx++];
            m_col++;
            if (c == '\n' || c == '\r') {
                if (!lastWasNewline) {
                    m_line++;
                    m_col = 0;
                    lastWasNewline = true;
                } else lastWasNewline = false;
                last = c;
                out.text += c;
                continue;
            } else lastWasNewline = false;

            if (quote && ((c != '\'' && c != '"') || last == '\\')) {
                last = c;
                out.text += c;
                continue;
            } else if (quote) {
                quote = token();
                last = c;
                out.text += c;
                continue;
            }

            if (c == '\'' || c == '"') {
                quote.line = m_line;
                quote.col = m_col;
                quote.text = "'";
                last = c;
                out.text += c;
                continue;
            }

            if (c == '(') {
                p_level++;
                parens.push({ m_line, m_col, "(" });
            }
            else if (c == ')') {
                if (p_level == 0) {
                    m_idx--;
                    break;
                }
                parens.pop();
                out.text += c;
                p_level--;
                continue;
            } else if (c == ',' && p_level == 0) {
                m_idx--;
                break;
            } else if (c == ';') {
                m_idx--;
                break;
            }

            last = c;
            out.text += c;
        }

        if (quote) {
            if (!expected) commit_state();
            throw parse_exception(
                module,
                "Encountered unexpected end of file while parsing string constant",
                lines[quote.line],
                quote.line,
                quote.col
            );
        }

        if (parens.size() > 0) {
            if (!expected) commit_state();
            throw parse_exception(
                module,
                "Encountered unexpected end of file while parsing expression",
                lines[parens.top().line],
                parens.top().line,
                parens.top().col
            );
        }

        if (expected && !out) {
            throw parse_exception(
                module,
                "Expected expression",
                lines[out.line],
                out.line,
                out.col
            );
        }

        if (!expected && !out) commit_state();

        return out;
    }

    tokenizer::token tokenizer::string_constant(bool expected, bool strip_quotes, bool allow_empty) {
        whitespace();
        token out = { m_line, m_col, "", module };

        token bt = character('\'', expected);
        if (!bt) return token();

        if (!strip_quotes) out.text += '\'';
        bool foundEnd = false;
        bool lastWasNewline = false;
        while(!at_end(false)) {
            char last = 0;
            if (out.text.length() > 0) {
                last = out.text[out.text.length() - 1];
            }

            if (m_input[m_idx] == '\'' && last != '\\') {
                m_idx++;
                m_col++;
                if (!strip_quotes) out.text += '\'';
                foundEnd = true;
                break;
            }

            char c = m_input[m_idx++];
            out.text += c;
            m_col++;

            if (c == '\n' || c == '\r') {
                if (!lastWasNewline) {
                    lastWasNewline = true;
                    m_line++;
                    m_col = 0;
                } else lastWasNewline = false;
            } else lastWasNewline = false;
        }

        if (!foundEnd) {
            throw parse_exception(
                module,
                "Encountered unexpected end of file while parsing string constant",
                lines[bt.line],
                bt.line,
                bt.col
            );
        }

        if (!allow_empty && out.text.length() == 0) {
            throw parse_exception(
                module,
                "String should not be empty",
                lines[bt.line],
                bt.line,
                bt.col
            );
        }

        return out;
    }

    tokenizer::token tokenizer::number_constant(bool expected) {
        whitespace();
        token out = { m_line, m_col, "", module };

        bool isNeg = false;
        bool hasDecimal = false;
        bool is_f32 = false;
        size_t offset = 0;
        size_t restore_idx = m_idx;
        u32 restore_col = m_col;
        u32 restore_line = m_line;

        while (!at_end()) {
            char c = m_input[m_idx + offset];
            if (c == '-') {
                if (out.text.length() != 0) break;
                isNeg = true;
                out.text += c;
                offset++;
            } else if (c == '.' && !hasDecimal) {
                if (out.text.length() == 0) break;
                hasDecimal = true;
                out.text += c;
                restore_idx = m_idx + offset;
                restore_col = m_col + offset;
                restore_line = m_line;
                offset++;
            } else if (c >= 48 && c <= 57) {
                out.text += c;
                offset++;
            } else if (c == 'f' && !is_f32) {
                is_f32 = true;
                out.text += c;
                offset++;
            } else {
                if (out.text.length() == 0) {
                    if (!expected) return token();
                    throw parse_exception(
                        module,
                        format("Expected numerical constant, found '%s'", thing().c_str()),
                        lines[m_line],
                        m_line,
                        m_col
                    );
                }
                break;
            }
        }

        m_idx += offset;
        m_col += (u32)offset;

        if (out.text.length() > 0 && out.text.back() == '.') {
            // numbers ending with a decimal are not valid decimals
            m_idx = restore_idx;
            m_col = restore_col;
            m_line = restore_line;
            out.text.pop_back();
        }

        return out;
    }
    
    tokenizer::token tokenizer::line_comment() {
        whitespace();
        if (m_idx + 1ull >= m_input.size()) return token();
        if (m_input[m_idx] != '/' && m_input[m_idx + 1ull] != '/') return token();
        token out = { m_line, m_col, "", module };
        while (!at_end(false)) {
            if (m_input[m_idx] == '\n' || m_input[m_idx] == '\r') break;
            out.text += m_input[m_idx];
            m_idx++;
            m_col++;
        }
        return out;
    }

    tokenizer::token tokenizer::block_comment() {
        whitespace();
        if (m_idx + 1ull >= m_input.size()) return token();
        if (m_input[m_idx] != '/' && m_input[m_idx + 1ull] != '*') return token();
        token out = { m_line, m_col, "", module };
        bool lastWasNewLine = false;
        while (!at_end(false)) {
            if (m_idx + 1ull >= m_input.size()) break;

            if (m_input[m_idx] == '*' && m_input[m_idx + 1ull] == '/') {
                out.text += "*/";
                m_idx += 2;
                break;
            }

            out.text += m_input[m_idx];
            m_idx++;
            m_col++;

            if (m_input[m_idx] == '\n' || m_input[m_idx] == '\r') {
                if (!lastWasNewLine) {
                    m_col = 0;
                    m_line++;
                    lastWasNewLine = true;
                } else lastWasNewLine = false;
            } else lastWasNewLine = false;
        }

        return out;
    }
};