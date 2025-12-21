#include <iostream>
#include <string>
#include <vector>
#include <cctype>

char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

bool is_vowel(char c) {
    c = to_lower(c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

bool is_consonant(char c) {
    return !is_vowel(c) && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

std::string remove_suffix(const std::string& word, const std::string& suffix) {
    if (word.length() >= suffix.length() &&
        word.substr(word.length() - suffix.length()) == suffix) {
        return word.substr(0, word.length() - suffix.length());
    }
    return word;
}

bool has_vowel_before_last_consonant(const std::string& word) {
    if (word.length() < 2) return false;
    for (int i = word.length() - 2; i >= 0; --i) {
        if (is_vowel(word[i])) return true;
        if (is_consonant(word[i])) break;
    }
    return false;
}

std::string stem_word(const std::string& word) {
    if (word.length() < 3) return word;

    std::string w = word;
    bool changed = true;
    while (changed) {
        changed = false;
        if (w.length() > 2 && w.back() == 's') {
            if (w.length() > 3 && w.substr(w.length() - 3) == "ies") {
                w = w.substr(0, w.length() - 3) + "i";
                changed = true;
            } else if (w.length() > 2 && w.substr(w.length() - 2) == "es") {
                w = w.substr(0, w.length() - 2);
                changed = true;
            } else if (w.length() > 1 && w.substr(w.length() - 1) == "s" && w[w.length() - 2] != 's') {
                w = w.substr(0, w.length() - 1);
                changed = true;
            }
        }

        if (w.length() > 3) {
            if (w.substr(w.length() - 3) == "ing") {
                w = w.substr(0, w.length() - 3);
                if (has_vowel_before_last_consonant(w)) {
                    if (w.length() > 1 && w.back() == w[w.length() - 2]) {
                        w = w.substr(0, w.length() - 1);
                    }
                }
                changed = true;
            } else if (w.substr(w.length() - 2) == "ed") {
                w = w.substr(0, w.length() - 2);
                if (has_vowel_before_last_consonant(w)) {
                    if (w.length() > 1 && w.back() == w[w.length() - 2]) {
                        w = w.substr(0, w.length() - 1);
                    }
                }
                changed = true;
            }
        }

        if (w.length() > 2) {
            if (w.substr(w.length() - 2) == "ly") {
                w = w.substr(0, w.length() - 2);
                changed = true;
            } else if (w.substr(w.length() - 4) == "ness") {
                w = w.substr(0, w.length() - 4);
                changed = true;
            } else if (w.substr(w.length() - 3) == "ful") {
                w = w.substr(0, w.length() - 3);
                changed = true;
            }
        }

        if (w.length() > 1 && w.back() == 'e') {
            if (has_vowel_before_last_consonant(w.substr(0, w.length() - 1))) {
                w = w.substr(0, w.length() - 1);
                changed = true;
            }
        }
    }

    return w;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <токен>\n";
        return 1;
    }

    std::string token = argv[1];
    std::string stemmed = stem_word(token);

    std::cout << stemmed << '\n';

    return 0;
}