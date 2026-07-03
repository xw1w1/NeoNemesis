#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <cctype>

class Json
{
public:
    std::unordered_map<std::string, std::string> fields; // key: string_value
    std::unordered_map<std::string, int64_t> int_fields; // key: int_value
    std::unordered_map<std::string, std::vector<std::string>> array_fields; // key: [str, str, ...]

    static bool Parse(const std::string& text, Json& out)
    {
        size_t i = 0;
        SkipWhitespace(text, i);
        if (i >= text.size() || text[i] != '{') return false;
        i++;

        while (i < text.size())
        {
            SkipWhitespace(text, i);
            if (i >= text.size()) return false;
            if (text[i] == '}') { i++; return true; }

            // key read
            std::string key;
            if (!ParseString(text, i, key)) return false;

            SkipWhitespace(text, i);
            if (i >= text.size() || text[i] != ':') return false;
            i++;
            SkipWhitespace(text, i);

            // value read
            if (i >= text.size()) return false;

            if (text[i] == '"')
            {
                std::string value;
                if (!ParseString(text, i, value)) return false;
                out.fields[key] = value;
            }
            else if (text[i] == '[')
            {
                std::vector<std::string> arr;
                if (!ParseArray(text, i, arr)) return false;
                out.array_fields[key] = arr;
            }
            else if (text[i] == '-' || std::isdigit((unsigned char)text[i]))
            {
                int64_t num;
                if (!ParseNumber(text, i, num)) return false;
                out.int_fields[key] = num;
            }
            else if (text.substr(i, 4) == "true") { out.int_fields[key] = 1; i += 4; }
            else if (text.substr(i, 5) == "false") { out.int_fields[key] = 0; i += 5; }
            else if (text.substr(i, 4) == "null") { i += 4; }
            else return false;

            SkipWhitespace(text, i);
            if (i < text.size() && text[i] == ',') { i++; continue; }
        }
        return false;
    }

    std::string GetString(const std::string& key, const std::string& def = "") const {
        auto it = fields.find(key);
        return (it != fields.end()) ? it->second : def;
    }
    int64_t GetInt(const std::string& key, int64_t def = 0) const {
        auto it = int_fields.find(key);
        return (it != int_fields.end()) ? it->second : def;
    }
    std::vector<std::string> GetArray(const std::string& key) const {
        auto it = array_fields.find(key);
        return (it != array_fields.end()) ? it->second : std::vector<std::string>{};
    }

private:
    static void SkipWhitespace(const std::string& s, size_t& i) {
        while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
    }

    static bool ParseString(const std::string& s, size_t& i, std::string& out) {
        if (i >= s.size() || s[i] != '"') return false;
        i++;
        out.clear();
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char c = s[i + 1];
                if (c == 'n') out += '\n';
                else if (c == 't') out += '\t';
                else if (c == '\\') out += '\\';
                else if (c == '"') out += '"';
                else out += c;
                i += 2;
            }
            else {
                out += s[i++];
            }
        }
        if (i >= s.size()) return false;
        i++; // закрывающая "
        return true;
    }

    static bool ParseNumber(const std::string& s, size_t& i, int64_t& out) {
        size_t start = i;
        if (s[i] == '-') i++;
        while (i < s.size() && std::isdigit((unsigned char)s[i])) i++;
        try { out = std::stoll(s.substr(start, i - start)); return true; }
        catch (...) { return false; }
    }

    static bool ParseArray(const std::string& s, size_t& i, std::vector<std::string>& out) {
        if (s[i] != '[') return false;
        i++;
        SkipWhitespace(s, i);
        while (i < s.size() && s[i] != ']') {
            std::string val;
            if (s[i] == '"') {
                if (!ParseString(s, i, val)) return false;
            }
            else {
                // Числа как строки для универсальности
                size_t start = i;
                while (i < s.size() && s[i] != ',' && s[i] != ']' && !std::isspace((unsigned char)s[i])) i++;
                val = s.substr(start, i - start);
            }
            out.push_back(val);
            SkipWhitespace(s, i);
            if (i < s.size() && s[i] == ',') { i++; SkipWhitespace(s, i); }
        }
        if (i < s.size()) i++;
        return true;
    }

public:
    static bool ParseArray(const std::string& text, std::vector<Json>& out)
    {
        out.clear();
        size_t i = 0;
        SkipWhitespace(text, i);
        if (i >= text.size() || text[i] != '[') return false;
        i++;
        SkipWhitespace(text, i);

        while (i < text.size() && text[i] != ']')
        {
            size_t obj_start = i;
            int depth = 0;
            bool in_string = false;

            while (i < text.size())
            {
                char c = text[i];
                if (c == '"' && (i == 0 || text[i - 1] != '\\'))
                {
                    in_string = !in_string;
                }
                else if (!in_string)
                {
                    if (c == '{') depth++;
                    else if (c == '}') { depth--; if (depth == 0) { i++; break; } }
                }
                i++;
            }

            std::string obj_str = text.substr(obj_start, i - obj_start);
            Json obj;
            if (Parse(obj_str, obj)) out.push_back(obj);

            SkipWhitespace(text, i);
            if (i < text.size() && text[i] == ',') { i++; SkipWhitespace(text, i); }
        }
        return true;
    }
};