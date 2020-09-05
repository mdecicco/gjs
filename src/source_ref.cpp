#include <source_ref.h>

namespace gjs {
    source_ref::source_ref() : line(0), col(0) { }
    source_ref::source_ref(const std::string& file, const std::string& line_txt, u16 _line, u16 _col) {
        filename = file;
        line_text = line_txt;
        line = _line;
        col = _col;
    }
    void source_ref::set(const std::string& file, const std::string& line_txt, u16 _line, u16 _col) {
        filename = file;
        line_text = line_txt;
        line = _line;
        col = _col;
    }
};