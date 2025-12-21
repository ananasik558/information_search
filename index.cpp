#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
    #include <dirent.h> 
#else
    #include <dirent.h>
#endif

char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current_token;

    for (char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            current_token += to_lower(c);
        } else {
            if (!current_token.empty() && current_token.length() >= 2) {
                tokens.push_back(current_token);
            }
            current_token.clear();
        }
    }

    if (!current_token.empty() && current_token.length() >= 2) {
        tokens.push_back(current_token);
    }

    return tokens;
}

struct TermRecord {
    std::string term;
    std::vector<int> doc_ids;
};

struct DocRecord {
    int doc_id;
    std::string title;
    std::string url;
};

bool compare_terms(const TermRecord& a, const TermRecord& b) {
    return a.term < b.term;
}

std::vector<std::string> list_txt_files(const std::string& dir) {
    std::vector<std::string> files;
    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        std::cerr << "Ошибка: не удалось открыть директорию " << dir << std::endl;
        return files;
    }

    struct dirent* ep;
    while ((ep = readdir(dp))) {
        std::string name = ep->d_name;
        if (name == "." || name == "..") continue;
        if (name.length() > 4 && name.substr(name.length() - 4) == ".txt") {
            files.push_back(name);
        }
    }
    closedir(dp);
    std::sort(files.begin(), files.end());
    return files;
}

int main() {
    const std::string corpus_dir = "corpus_en";
    const std::string inverted_index_file = "inverted_index.bin";
    const std::string forward_index_file = "forward_index.bin";

    std::vector<TermRecord> inverted_index;
    std::vector<DocRecord> forward_index;

    std::vector<std::string> filenames = list_txt_files(corpus_dir);
    if (filenames.empty()) {
        std::cerr << "Нет файлов в " << corpus_dir << std::endl;
        return 1;
    }

    size_t num_files = filenames.size();
    std::cout << "Найдено " << num_files << " файлов\n";

    for (size_t doc_id = 0; doc_id < num_files; ++doc_id) {
        const std::string& filename = filenames[doc_id];
        std::string filepath = corpus_dir + "/" + filename;

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Не удалось открыть: " << filepath << std::endl;
            continue;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();
        file.close();

        std::vector<std::string> tokens = tokenize(content);

        DocRecord doc_rec;
        doc_rec.doc_id = static_cast<int>(doc_id);
        size_t dot_pos = filename.find('.');
        doc_rec.title = (dot_pos != std::string::npos) ? filename.substr(0, dot_pos) : filename;
        doc_rec.url = "https://en.wikipedia.org/wiki/" + doc_rec.title;
        forward_index.push_back(doc_rec);

        for (const std::string& token : tokens) {
            bool found = false;
            for (TermRecord& term_rec : inverted_index) {
                if (term_rec.term == token) {
                    term_rec.doc_ids.push_back(static_cast<int>(doc_id));
                    found = true;
                    break;
                }
            }
            if (!found) {
                TermRecord new_term;
                new_term.term = token;
                new_term.doc_ids.push_back(static_cast<int>(doc_id));
                inverted_index.push_back(new_term);
            }
        }

        if ((doc_id + 1) % 1000 == 0) {
            std::cout << "Обработано: " << (doc_id + 1) << " документов\n";
        }
    }

    std::sort(inverted_index.begin(), inverted_index.end(), compare_terms);

    std::ofstream inv_out(inverted_index_file, std::ios::binary);
    if (!inv_out.is_open()) {
        std::cerr << "Не удалось создать " << inverted_index_file << std::endl;
        return 1;
    }

    int num_terms = static_cast<int>(inverted_index.size());
    inv_out.write(reinterpret_cast<const char*>(&num_terms), sizeof(int));

    for (const TermRecord& tr : inverted_index) {
        int len = static_cast<int>(tr.term.length());
        inv_out.write(reinterpret_cast<const char*>(&len), sizeof(int));
        inv_out.write(tr.term.c_str(), len);

        int num_docs = static_cast<int>(tr.doc_ids.size());
        inv_out.write(reinterpret_cast<const char*>(&num_docs), sizeof(int));
        for (int id : tr.doc_ids) {
            inv_out.write(reinterpret_cast<const char*>(&id), sizeof(int));
        }
    }
    inv_out.close();

    std::ofstream fwd_out(forward_index_file, std::ios::binary);
    if (!fwd_out.is_open()) {
        std::cerr << "Не удалось создать " << forward_index_file << std::endl;
        return 1;
    }

    int num_docs = static_cast<int>(forward_index.size());
    fwd_out.write(reinterpret_cast<const char*>(&num_docs), sizeof(int));

    for (const DocRecord& dr : forward_index) {
        fwd_out.write(reinterpret_cast<const char*>(&dr.doc_id), sizeof(int));

        int len_title = static_cast<int>(dr.title.length());
        fwd_out.write(reinterpret_cast<const char*>(&len_title), sizeof(int));
        fwd_out.write(dr.title.c_str(), len_title);

        int len_url = static_cast<int>(dr.url.length());
        fwd_out.write(reinterpret_cast<const char*>(&len_url), sizeof(int));
        fwd_out.write(dr.url.c_str(), len_url);
    }
    fwd_out.close();

    std::cout << "\nИндексы построены успешно.\n";
    std::cout << "Документов: " << num_docs << "\n";
    std::cout << "Терминов: " << num_terms << "\n";

    return 0;
}