#ifndef TOKEN_STR_H
#define TOKEN_STR_H

#define MAX_NB_TOKENS 256

typedef struct {
    char* tokens[MAX_NB_TOKENS];
    int nb_tokens;
} token_str_t;

token_str_t* token_str_tokenize(char* str, const char* delim);

void token_str_destroy(token_str_t* token_str);

#endif
