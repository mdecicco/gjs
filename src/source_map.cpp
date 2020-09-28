#include <source_map.h>
#include <parse.h>
using namespace std;

namespace gjs {
	void source_map::append(ast_node* node) {
		append(node->start.file, node->start.lineText, node->start.line, node->start.col);
	}

	void source_map::append(const std::string& file, const std::string& lineText, u32 line, u32 col) {
		elem e;
		bool found_file = false;
		bool found_line = false;
		for (u16 i = 0;i < files.size();i++) {
			if (files[i] == file) {
				e.file = i;
				found_file = true;
			}
		}
		for (u32 i = 0;i < lines.size();i++) {
			if (lines[i] == lineText) {
				e.lineTextIdx = i;
				found_line = true;
			}
		}
		if (!found_file) {
			e.file = files.size();
			files.push_back(file);
		}
		if (!found_line) {
			e.lineTextIdx = lines.size();
			lines.push_back(lineText);
		}

		e.line = line;
		e.col = col;

		map.push_back(e);
	}

	source_map::src_info source_map::get(address addr) {
		elem e = map[addr];
		return { files[e.file], lines[e.lineTextIdx], e.line, e.col };
	}
};