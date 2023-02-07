#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <iostream>

using namespace std;

namespace parse {

    const int NUM_OF_SPACES_IN_INDENT = 2;

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input) : input_(input)
    {
        FindNextToken();
    }

    const Token& Lexer::CurrentToken() const
    {
        return tokens_.back();
    }

    Token Lexer::NextToken()
    {
        FindNextToken();
        return tokens_.back();
    }

    void Lexer::ProcEndStream()
    {
        if(is_start_line && cur_indent > 0)
        {
            if(cur_indent < indent)
            {
                ++cur_indent;
                tokens_.emplace_back(token_type::Indent{});
            }
            else if(cur_indent > indent)
            {
                --cur_indent;
                tokens_.emplace_back(token_type::Dedent{});
            }
        }
        else
        {
            if(!is_start_line)
            {
                tokens_.emplace_back(token_type::Newline{});
                is_start_line = true;
            }
            else
            {
                tokens_.emplace_back(token_type::Eof{});
            }
        }
    }

    void Lexer::ProcSpace()
    {
        int space_count = 1;
        char next_char = input_.peek();
        while(next_char == ' ')
        {
            input_.get();
            next_char = input_.peek();
            ++space_count;
        }

        if(is_start_line)
        {
            indent = space_count / NUM_OF_SPACES_IN_INDENT;
        }

        FindNextToken();
    }

    void Lexer::ProcEndOfLine()
    {
        if(is_start_line)
        {
            is_start_line = true;
            FindNextToken();
        }
        else
        {
            indent = 0;
            is_start_line = true;
            tokens_.emplace_back(token_type::Newline{});
        }
    }

    void Lexer::ProcIndent()
    {
        if (cur_indent < indent)
        {
            ++cur_indent;
            tokens_.emplace_back(token_type::Indent{});
            input_.unget();
        }
        else if(cur_indent > indent)
        {
            --cur_indent;
            tokens_.emplace_back(token_type::Dedent{});
            input_.unget();
        }
    }

    void Lexer::ProcNumber(char cur_char)
    {
        std::string number;
        number += cur_char;

        char next_char = input_.peek();
        while(std::isdigit(next_char))
        {
            number += next_char;
            input_.get();
            next_char = input_.peek();
        }

        tokens_.emplace_back(token_type::Number{std::stoi(number)});
        is_start_line = false;
    }

    void Lexer::ProcWord(char cur_char)
    {
        std::string word;
        word = cur_char;

        char next_char = input_.peek();
        while(next_char == '_' || std::isalpha(next_char) || std::isdigit(next_char))
        {
            word += next_char;
            input_.get();
            next_char = input_.peek();
        }

        if(keywords_.find(word) != keywords_.end())
        {
            tokens_.push_back(keywords_[word]);
        }
        else
        {
            tokens_.emplace_back(token_type::Id{word});
        }

        is_start_line = false;
    }

    void Lexer::ProcShielding(char cur_char)
    {
        std::string str;

        char quotes = cur_char;
        char next_char = input_.peek();
        while(next_char != quotes)
        {
            if(next_char == '\\')
            {
                input_.get();
                next_char = input_.peek();
                if(next_char == '\'')
                {
                    str += '\'';
                }
                else if(next_char == '\"')
                {
                    str += '\"';
                }
                else if(next_char == 'n')
                {
                    str += '\n';
                }
                else if(next_char == 't')
                {
                    str += '\t';
                }
                input_.get();
                next_char = input_.peek();
            }
            else
            {
                str+= next_char;
                input_.get();
                next_char = input_.peek();
            }
        }

        input_.get();
        tokens_.emplace_back(token_type::String{str});
        is_start_line = false;
    }

    void Lexer::ProcComment()
    {
        std::string str;
        std::getline(input_, str);

        char next_char = input_.peek();

        if(next_char != EOF)
        {
            input_.putback('\n');
            FindNextToken();
        }
        else
        {
            tokens_.emplace_back(token_type::Eof{});
        }
    }

    void Lexer::FindNextToken()
    {
        char cur_char = input_.get();

        if(cur_char == EOF)
        {
            ProcEndStream();
        }
        else if(cur_char == ' ')
        {
            ProcSpace();
        }
        else if(cur_char == '\n')
        {
            ProcEndOfLine();
        }
        else if(cur_indent != indent && is_start_line)
        {
            ProcIndent();
        }
        else if(std::isdigit(cur_char))
        {
            ProcNumber(cur_char);
        }
        else if(cur_char == '_' || std::isalpha(cur_char) || std::isdigit(cur_char))
        {
            ProcWord(cur_char);
        }
        else if(cur_char == '\'' || cur_char == '\"')
        {
            ProcShielding(cur_char);
        }
        else if(cur_char == '#')
        {
            ProcComment();
        }
        else if(cur_char == '=' && input_.peek() == '=')
        {
            input_.get();
            tokens_.emplace_back(token_type::Eq{});
        }
        else if(cur_char == '!' && input_.peek() == '=')
        {
            input_.get();
            tokens_.emplace_back(token_type::NotEq{});
        }
        else if(cur_char == '<' && input_.peek() == '=')
        {
            input_.get();
            tokens_.emplace_back(token_type::LessOrEq{});
        }
        else if(cur_char == '>' && input_.peek() == '=')
        {
            input_.get();
            tokens_.emplace_back(token_type::GreaterOrEq{});
        }
        else if(std::ispunct(cur_char))
        {
            tokens_.emplace_back(token_type::Char{cur_char});
            is_start_line = false;
        }
    }

}  // namespace parse
