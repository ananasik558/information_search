#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

char to_lower_ascii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

bool is_alphanum(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current_token;

    for (char c : text) {
        if (is_alphanum(c)) {
            current_token += to_lower_ascii(c);
        } else {
            if (current_token.length() >= 2) {
                tokens.push_back(current_token);
            }
            current_token.clear();
        }
    }

    if (current_token.length() >= 2) {
        tokens.push_back(current_token);
    }

    return tokens;
}
std::string read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
        return "";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <файл.txt>\n";
        return 1;
    }

    std::string filename = argv[1];
    std::string content = read_file(filename);
    if (content.empty()) {
        return 1;
    }

    std::vector<std::string> tokens = tokenize(content);

    for (const std::string& token : tokens) {
        std::cout << token << '\n';
    }

    return 0;
}