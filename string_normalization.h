#ifndef STRING_NORMALIZATION_H
#define STRING_NORMALIZATION_H

char *normalize_string(const char *input);
void change_to_dot_format(unsigned char *str);
void change_to_dns_format(char *src, unsigned char *dest);
char *main_domain_finder(const char *input);
#endif // STRING_NORMALIZATION_H