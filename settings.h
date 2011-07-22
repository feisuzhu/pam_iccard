char *get_setting(const char *o);
int parse_file(const char *fn,int preserve);
void parse_args(int argc,char **argv,int preserve);

#define on get_setting
#define off(x) (!get_setting(x))

