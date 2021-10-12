#include <stdlib.h>
#include <string.h>

#include "token_str.h"

token_str_t*
token_str_tokenize(char* str, const char* delim)
{
    token_str_t* token_str = calloc(1, sizeof(token_str_t));
    char*        token     = strtok(str, delim);
    int          i         = 0;
    while (token != NULL && i < MAX_NB_TOKENS) {
        size_t token_len = strlen(token);
        token_str->tokens[i] = calloc(token_len + 1, sizeof(char));
        memcpy(token_str->tokens[i], token, token_len);
        token_str->nb_tokens++;
        token = strtok(NULL, " ");
        i++;
    }
    return token_str;
}

void
token_str_destroy(token_str_t* token_str)
{
    if (!token_str) {
        return;
    }

    for(int i = 0; i < token_str->nb_tokens; i++) {
        free(token_str->tokens[i]);
    }
    free(token_str);
}
