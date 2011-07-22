typedef struct _token {
    int magic;      /* should be 'CARD' */
    int magic1;     /* should be 'CARD' and be encrypted if has PIN */
    int id;         
    char token[56];
} token;
    
typedef struct _userinfo {
    char username[32];
    int id;
    char hash[64];
    struct _userinfo *next;
} userinfo;

char *digest(void *in, int salt, char *out);
char *aes_decrypt(void *in, int len, void *out, const char *key);
char *aes_encrypt(void *in, int len, void *out, const char *key);
char *aes_op(void *in, int len, void *out, const char *key, int op);
int validate_token(token *tok, const char *pin);
void free_userinfo(userinfo *p);
userinfo *find_user_by_name(userinfo *p, const char *username);
userinfo *find_user_by_id(userinfo *p, int id);
int write_userinfo(userinfo *p);
userinfo *read_userinfo();
char *dehex(char *in, int len, char *out);
char *hex(char *in, int len, char *out);
char *get_rand_bytes(void *out, int len);
int myrand();

/* CARD */
#define MAGIC 0x44524143
#define VALIDATE_TOKEN_NEEDPIN 1234

void card_setstrerror(const char *s);
const char *card_strerror();

/* vim: set fdm=syntax: */
