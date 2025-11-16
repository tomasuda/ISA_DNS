/**
 * @file string_normalization.h
 * @author Tomas Suda (xsudato00)
 * @date 20/9/2025
 */
#ifndef STRING_NORMALIZATION_H
#define STRING_NORMALIZATION_H

char *normalize_string(const char *input);
void change_to_dot_format(unsigned char *str);
void change_to_dns_format(char *src, unsigned char *dest);
char *main_domain_finder(const char *input);
char *domain_parser(char* input);
void to_lower_str(char *s);
#endif // STRING_NORMALIZATION_H