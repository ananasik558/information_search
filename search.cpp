#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cctype>
#include <chrono>

struct TermRecord {
    std::string term;
    std::vector<int> doc_ids;
};

struct DocRecord {
    int doc_id;
    std::string title;
    std::string url;
};

std::vector<std::string> tokenize_query(const std::string& query);
std::vector<int> evaluate_expression(const std::vector<std::string>& tokens, size_t& pos, const std::vector<TermRecord>& inverted_index, int total_docs);
std::vector<int> evaluate_term(const std::vector<std::string>& tokens, size_t& pos, const std::vector<TermRecord>& inverted_index, int total_docs);
std::vector<int> evaluate_factor(const std::vector<std::string>& tokens, size_t& pos, const std::vector<TermRecord>& inverted_index, int total_docs);
std::vector<int> execute_search(const std::string& query, const std::vector<TermRecord>& inverted_index, const std::vector<DocRecord>& forward_index);

char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

std::vector<std::string> tokenize_query(const std::string& query) {
    std::vector<std::string> tokens;
    std::string current_token;

    for (char c : query) {
        if (c == '(' || c == ')' || c == '|' || c == '&' || c == '!') {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
            if (c == '|' && !tokens.empty() && tokens.back() == "|") {
                tokens.pop_back();
                tokens.push_back("||");
            } else if (c == '&' && !tokens.empty() && tokens.back() == "&") {
                tokens.pop_back();
                tokens.push_back("&&");
            } else {
                tokens.push_back(std::string(1, c));
            }
        } else if (c == ' ') {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        } else {
            current_token += to_lower(c);
        }
    }
    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }
    return tokens;
}

std::vector<int> get_doc_ids(const std::string& term, const std::vector<TermRecord>& inverted_index) {
    for (const TermRecord& tr : inverted_index) {
        if (tr.term == term) {
            return tr.doc_ids;
        }
    }
    return {};
}

std::vector<int> and_op(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> result;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            result.push_back(a[i]);
            ++i;
            ++j;
        } else if (a[i] < b[j]) {
            ++i;
        } else {
            ++j;
        }
    }
    return result;
}

std::vector<int> or_op(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> result;
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i] == b[j]) {
            result.push_back(a[i]);
            ++i;
            ++j;
        } else if (a[i] < b[j]) {
            result.push_back(a[i]);
            ++i;
        } else {
            result.push_back(b[j]);
            ++j;
        }
    }
    while (i < a.size()) {
        result.push_back(a[i]);
        ++i;
    }
    while (j < b.size()) {
        result.push_back(b[j]);
        ++j;
    }
    return result;
}

std::vector<int> not_op(const std::vector<int>& a, int total_docs) {
    std::vector<int> result;
    std::set<int> excluded(a.begin(), a.end());
    for (int i = 0; i < total_docs; ++i) {
        if (excluded.find(i) == excluded.end()) {
            result.push_back(i);
        }
    }
    return result;
}

std::vector<int> evaluate_expression(const std::vector<std::string>& tokens, size_t& pos, const std::vector<TermRecord>& inverted_index, int total_docs) {
    std::vector<int> left = evaluate_term(tokens, pos, inverted_index, total_docs);
    while (pos < tokens.size() && tokens[pos] == "||") {
        ++pos;
        std::vector<int> right = evaluate_term(tokens, pos, inverted_index, total_docs);
        left = or_op(left, right);
    }
    return left;
}

std::vector<int> evaluate_term(const std::vector<std::string>& tokens, size_t& pos, const std::vector<TermRecord>& inverted_index, int total_docs) {
    std::vector<int> left = evaluate_factor(tokens, pos, inverted_index, total_docs);
    while (pos < tokens.size() && (tokens[pos] == "&&" || tokens[pos] == " ")) {
        ++pos;
        std::vector<int> right = evaluate_factor(tokens, pos, inverted_index, total_docs);
        left = and_op(left, right);
    }
    return left;
}

std::vector<int> evaluate_factor(const std::vector<std::string>& tokens, size_t& pos, const std::vector<TermRecord>& inverted_index, int total_docs) {
    if (pos >= tokens.size()) {
        return {};
    }

    if (tokens[pos] == "!") {
        ++pos;
        std::vector<int> operand = evaluate_factor(tokens, pos, inverted_index, total_docs);
        return not_op(operand, total_docs);
    }

    if (tokens[pos] == "(") {
        ++pos;
        std::vector<int> result = evaluate_expression(tokens, pos, inverted_index, total_docs);
        if (pos < tokens.size() && tokens[pos] == ")") {
            ++pos;
        }
        return result;
    }

    std::string term = tokens[pos];
    ++pos;
    return get_doc_ids(term, inverted_index);
}

std::vector<TermRecord> load_inverted_index(const std::string& filename) {
    std::vector<TermRecord> inverted_index;
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удаётся открыть " << filename << "\n";
        return inverted_index;
    }

    int num_terms;
    file.read(reinterpret_cast<char*>(&num_terms), sizeof(int));

    for (int i = 0; i < num_terms; ++i) {
        int len;
        file.read(reinterpret_cast<char*>(&len), sizeof(int));
        std::string term(len, '\0');
        file.read(&term[0], len);

        int num_docs;
        file.read(reinterpret_cast<char*>(&num_docs), sizeof(int));
        std::vector<int> doc_ids(num_docs);
        for (int j = 0; j < num_docs; ++j) {
            file.read(reinterpret_cast<char*>(&doc_ids[j]), sizeof(int));
        }

        inverted_index.push_back({term, doc_ids});
    }
    return inverted_index;
}

std::vector<DocRecord> load_forward_index(const std::string& filename) {
    std::vector<DocRecord> forward_index;
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удаётся открыть " << filename << "\n";
        return forward_index;
    }

    int num_docs;
    file.read(reinterpret_cast<char*>(&num_docs), sizeof(int));

    for (int i = 0; i < num_docs; ++i) {
        int doc_id;
        file.read(reinterpret_cast<char*>(&doc_id), sizeof(int));

        int len_title;
        file.read(reinterpret_cast<char*>(&len_title), sizeof(int));
        std::string title(len_title, '\0');
        file.read(&title[0], len_title);

        int len_url;
        file.read(reinterpret_cast<char*>(&len_url), sizeof(int));
        std::string url(len_url, '\0');
        file.read(&url[0], len_url);

        forward_index.push_back({doc_id, title, url});
    }
    return forward_index;
}

std::vector<int> execute_search(const std::string& query, const std::vector<TermRecord>& inverted_index, const std::vector<DocRecord>& forward_index) {
    std::vector<std::string> tokens = tokenize_query(query);
    size_t pos = 0;
    int total_docs = static_cast<int>(forward_index.size());
    return evaluate_expression(tokens, pos, inverted_index, total_docs);
}

void print_results_cli(const std::vector<int>& doc_ids, const std::vector<DocRecord>& forward_index) {
    for (int doc_id : doc_ids) {
        if (doc_id >= 0 && doc_id < static_cast<int>(forward_index.size())) {
            const DocRecord& dr = forward_index[doc_id];
            std::cout << dr.title << " | " << dr.url << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " \"запрос\"\n";
        return 1;
    }

    std::string query = argv[1];

    auto inverted_index = load_inverted_index("inverted_index.bin");
    auto forward_index = load_forward_index("forward_index.bin");

    if (inverted_index.empty() || forward_index.empty()) {
        std::cerr << "Не удалось загрузить индексы\n";
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto results = execute_search(query, inverted_index, forward_index);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Время выполнения: " << (duration / 1000.0) << " мс\n";
    std::cout << "Найдено: " << results.size() << " документов\n";
    print_results_cli(results, forward_index);

    return 0;
}