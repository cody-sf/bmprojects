#ifndef STRINGUTILS_H
#define STRINGUTILS_H

namespace StringUtils {

static inline bool strings_match(const char* str1, const char* str2)
{
    return strcmp(str1, str2) == 0;
}

} // namespace StringUtils

#endif // STRINGUTILS_H
