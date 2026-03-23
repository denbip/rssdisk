#ifndef JSON_ESCAPE_H
#define JSON_ESCAPE_H

#include <string>
#include <sstream>
#include <iomanip>

class json_escape
{
public:
    static std::string escape_if_unescaped(const std::string& input)
    {
        std::ostringstream ss;
        bool flag_escape { false };
        for (auto iter = input.cbegin(); iter != input.cend(); iter++)
        {
            switch (*iter)
            {
                case '\\':
                {
                    flag_escape = !flag_escape;
                    ss << *iter;
                    break;
                }
                case '"':
                {
                    if (!flag_escape) ss << "\\";
                    else flag_escape = false;

                    ss << *iter;
                    break;
                }
                case '\b':
                {
                    if (flag_escape)
                    {
                        ss << "\\";
                        flag_escape = false;
                    }
                    ss << "\\b";
                    break;
                }
                case '\f':
                {
                    if (flag_escape)
                    {
                        ss << "\\";
                        flag_escape = false;
                    }
                    ss << "\\f";
                    break;
                }
                case '\n':
                {
                    if (flag_escape)
                    {
                        ss << "\\";
                        flag_escape = false;
                    }
                    ss << "\\n";
                    break;
                }
                case '\r':
                {
                    if (flag_escape)
                    {
                        ss << "\\";
                        flag_escape = false;
                    }
                    ss << "\\r";
                    break;
                }
                case '\t':
                {
                    if (flag_escape)
                    {
                        ss << "\\";
                        flag_escape = false;
                    }
                    ss << "\\t";
                    break;
                }
                default:
                {
                    if ('\x00' <= *iter && *iter <= '\x1f')
                    {
                        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*iter;
                    }
                    else
                    {
                        if (flag_escape)
                        {
                            if (*iter == 'b' ||
                                *iter == 'f' ||
                                *iter == 'n' ||
                                *iter == 'r' ||
                                *iter == 't')
                            {

                            }
                            else
                            {
                                ss << "\\";
                            }
                            flag_escape = false;
                        }

                        ss << *iter;
                    }
                    break;
                }
            }
        }

        if (flag_escape) ss << "\\";

        return ss.str();
    }

    static std::string escape(const std::string& input)
    {
        std::ostringstream ss;
        for (auto iter = input.cbegin(); iter != input.cend(); iter++) {
        //C++98/03:
        //for (std::string::const_iterator iter = input.begin(); iter != input.end(); iter++) {
            switch (*iter) {
                case '\\': ss << "\\\\"; break;
                case '"': ss << "\\\""; break;
                //case '/': ss << "\\/"; break;
                case '\b': ss << "\\b"; break;
                case '\f': ss << "\\f"; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default:
                {
                    if ('\x00' <= *iter && *iter <= '\x1f')
                    {
                        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*iter;
                    }
                    else
                    {
                        ss << *iter;
                    }
                    break;
                }
            }
        }
        return ss.str();
    }

    static std::string escape_formed_json(const std::string& input)
    {
      int comma = 0, quota = 0, colon = 0, opnarr = 0, clsarr = 0;
      // remove whitespace
      std::ostringstream sc;
      for (auto iter = input.cbegin(); iter != input.cend(); iter++)
        switch (*iter) {
        case '{': case '}': sc << *iter; quota = colon = comma = opnarr = clsarr = 0; break;
        case '[': sc << '['; opnarr++; clsarr = 0; break;
        case ']': sc << ']'; clsarr++; opnarr = 0; break;
        case ':': sc << ':'; if (quota > 0 && opnarr == 0) { quota = 0; colon++; } break;
        case ',': sc << ','; if (quota > 0 && opnarr == 0) { quota = colon = 0; comma++;  } break;
        case '"': sc << '"'; quota++; break;
        case ' ': {
          auto val = *(iter+1);
          if ( (opnarr > 0 || colon > 0) && quota > 0 && val != ',' && val != '}' && val != ']' && val != ':' && val != ' ') sc << ' ';
          break; }
        default: {
          if ('\x00' <= *iter && *iter <= '\x1f') sc << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*iter;
          else sc << *iter;
          break; }
        }
      // escape characters
      std::string clear_input = sc.str();
      sc.str("");
      sc.clear();
      for (auto iter = clear_input.cbegin(); iter != clear_input.cend(); iter++)
        switch (*iter) {
        case '{': case '}': sc << *iter; quota = colon = comma = opnarr = clsarr = 0; break;
        case '[': sc << '['; opnarr++; clsarr = 0; break;
        case ']': sc << ']'; clsarr++; opnarr = 0; break;
        case ':': sc << ':'; if (quota > 0 && opnarr == 0) { quota = 0; colon++; } break;
        case ',': sc << ','; colon = 0; break;
        case '\\': {

          if (*(iter + 1) != '\0')
          {
            auto val = *(iter+1);
            if (val != '"') sc << "\\\\";
            else
            {
              if (*(iter + 2) != '\0')
              {
                auto val = *(iter+2);
                if ( val == ',' || val == '}' || val == ']') sc << "\\\\";
              }
              else
                sc << "\\" << *(++iter);
            }
          }
          else
          {
            sc << "\\\\";
          }
          break; }
        case '"': {
          auto val = *(iter+1);
          if ( (colon > 0 && opnarr == 0 && quota > 0 && val != ',' && val != '}' && val != ']') ) sc << "\\\"";
          else if (opnarr > 0 && quota > 0 && val != ',' && val != '}' && val != ']') {sc << "\\\""; }
          else if ((opnarr > 0 && quota > 0) && (val == ',' || val == '}' || val != ']')) { sc << "\""; quota = 0; }
          else { sc << "\""; quota++; }
          break; }
        default: {
          if ('\x00' <= *iter && *iter <= '\x1f') sc << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)*iter;
          else sc << *iter;
          break; }
        }
      return sc.str();
    }
};

#endif // JSON_ESCAPE_H
