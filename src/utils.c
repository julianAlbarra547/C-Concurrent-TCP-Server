#include "utils.h"
 
void trim(char *text){
    if(!text) return;
 
    char *start = text;
    while(*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')){
        start++;
    }
 
    char *end = text + strlen(text);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')){
        end--;
    }
 
    size_t len = (size_t)(end - start);
    if(start != text) memmove(text, start, len);
    text[len] = '\0';
}
 
int prompt_text(const char *label, char *out, size_t max_size){
    printf("%s", label);
    fgets(out, max_size, stdin);
    trim(out);
 
    if(strncmp(out, "0", 1) == 0){
        printf("Regresando al menu principal...\n");
        return 0;
    }
 
    return 1;
}
 
int valid_date(char *date){
    if(strlen(date) != 10)  return 0;
    if(date[4] != '-')      return 0;
    if(date[7] != '-')      return 0;
 
    for(int i = 0; i < 10; i++){
        if(i == 4 || i == 7) continue;
        if(!isdigit(date[i])) return 0;
    }
 
    int month = (date[5] - '0') * 10 + (date[6] - '0');
    int day   = (date[8] - '0') * 10 + (date[9] - '0');
 
    if(month < 1 || month > 12) return 0;
    if(day   < 1 || day   > 31) return 0;
 
    return 1;
}
 
int valid_explicit(char *explicito){
    if(strcmp(explicito, "True")  == 0) return 1;
    if(strcmp(explicito, "False") == 0) return 1;
    return 0;
}
 
int valid_positive_int(char *buff){
    for(int i = 0; buff[i] != '\0'; i++){
        if(!isdigit(buff[i])) return 0;
    }
    return atoi(buff) > 0;
}
 
int valid_positive_double(char *buff){
    int dots = 0;
    for(int i = 0; buff[i] != '\0'; i++){
        if(buff[i] == '.'){
            dots++;
            if(dots > 1) return 0;
            continue;
        }
        if(!isdigit(buff[i])) return 0;
    }
    return atof(buff) > 0;
}
