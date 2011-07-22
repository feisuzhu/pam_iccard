#include <openssl/evp.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <security/_pam_macros.h>

#include <libintl.h>
#define _ gettext

#include "common.h"

char *hex(char *in, int len, char *out)
{
    const char *a = "ABCDEFGHIJKLMNOP"; /* for convenience */
    int i;

    if(!out)
        out = malloc((len<<1)+1);
    i = len;
    while(i--) {
        out[i<<1] = a[(in[i] >> 4) & 0xF];
        out[(i<<1)+1] = a[in[i] & 0xF];
    }
    out[(len<<1)] = 0;
    return out;
}

char *dehex(char *in, int len, char *out)
{
    int i;

    if(len & 1)
        return NULL;
    
    i = len>>1;
    if(!out)
        out = malloc(i);
    while(i--) {
        out[i] = ((in[i<<1] - 'A') << 4) | (in[(i<<1)+1] -'A');
    }
    return out;
}


userinfo *read_userinfo()
{
    userinfo *head, *cur;
    char buf[1000];
    FILE *f;

    f = fopen("/etc/cards", "r");

    head = malloc(sizeof(userinfo));
    memset(head, 0, sizeof(userinfo));
    strcpy(head->username, "+DUMMY");
    cur = head;

    while(fgets(buf, sizeof buf, f)) {
        cur->next = malloc(sizeof(userinfo));
        cur = cur->next;
        memset(cur, 0, sizeof(userinfo));

        strcpy(cur->username, strtok(buf, ":"));
        dehex(strtok(NULL, ":"), sizeof(cur->id)<<1, (char*)&cur->id);
        dehex(strtok(NULL, "\n"), sizeof(cur->hash)<<1, cur->hash);
    }

    //last:
    memset(buf, 0, sizeof buf);
    if(f)
        fclose(f);

    return head;
}

int write_userinfo(userinfo *p)
{
    FILE *f;
    char buf0[4], buf2[130];
    f = fopen("/etc/cards", "w");
    
    if(!f) return 0;

    /* first one should be a dummy */
    for(p = p->next; p; p = p->next) {
        fprintf(f, "%s:%s:%s\n",
            p->username,
            hex((char*)&p->id, sizeof(p->id), buf0),
            hex(p->hash, sizeof(p->hash), buf2)
        );
    }
    
    fchmod(fileno(f), 0644);
    //last:
    if(f)
        fclose(f);
    return 1;
}

userinfo *find_user_by_name(userinfo *p, const char *username)
{
    //if(!p) return NULL;

    for(p = p->next; p; p = p->next) {
        if(!strcmp(p->username, username))
            break;
    }
    return p;
}

userinfo *find_user_by_id(userinfo *p, int id)
{
    //if(!p) return NULL;

    for(p = p->next; p; p = p->next) {
        if(p->id == id)
            break;
    }
    return p;
}

void free_userinfo(userinfo *p)
{
    userinfo *n;
    while(p) {
        n = p->next;
        free(p);
        p = n;
    }
}

char *aes_op(void *in, int len, void *out, const char *key, int op)
{
    EVP_CIPHER_CTX ctx;
    EVP_MD_CTX mdctx;

    char md[16];
    int ol, i;

    if(len & 0xF) /* well, not aligned */
        return NULL;
    
    if(!out)
        out = malloc(len);
    
    /* prepare for encryption key */
    memset(md, 0, sizeof(md));
    strncpy(md, key, sizeof(md));

    EVP_MD_CTX_init(&mdctx);
    i = 20000;
    while(i--) {
        EVP_DigestInit_ex(&mdctx, EVP_md5(), NULL);
        EVP_DigestUpdate(&mdctx, (unsigned char *)md, sizeof(md));
        EVP_DigestFinal_ex(&mdctx, (unsigned char *)md, NULL);
    }
    EVP_MD_CTX_cleanup(&mdctx);


    EVP_CIPHER_CTX_init(&ctx);
    EVP_CipherInit_ex(
        &ctx, /* context */
        EVP_aes_128_cbc(), /* cipher */
        NULL, /* impl */
        (unsigned char *)md, /* key */
        (unsigned char *)"Proton rocks!!!!", /* iv */
        op /* 1 for encryption, 0 for decryption, -1 for nop */
    );
    EVP_CIPHER_CTX_set_padding(&ctx, 0);

    EVP_CipherUpdate(&ctx, out, &ol, in, len);
    EVP_CipherFinal_ex(&ctx, out, &ol);
    
    if(ol) exit(1); /* fuck, sth is wrong */
    EVP_CIPHER_CTX_cleanup(&ctx);

    return out;
}

char *aes_encrypt(void *in, int len, void *out, const char *key)
{
    return aes_op(in, len, out, key, 1);
}

char *aes_decrypt(void *in, int len, void *out, const char *key)
{
    return aes_op(in, len, out, key, 0);   
}

/* SHA512, 512Bits = 64Bytes */
char *digest(void *in, int salt, char *out)
{
    EVP_MD_CTX mdctx;
    int i;

    if(!out)
        out = malloc(64);

    memcpy(out, in, 64);
    EVP_MD_CTX_init(&mdctx);
    i = 20000;
    while(i--) {
        EVP_DigestInit_ex(&mdctx, EVP_sha512(), NULL);
        EVP_DigestUpdate(&mdctx, (void *)out, 64);
        EVP_DigestFinal_ex(&mdctx, (void *)out, NULL);
        ((unsigned int *)out)[i & 15] ^= (unsigned int )salt;
    }
    EVP_MD_CTX_cleanup(&mdctx);
    return out;
}

char *get_rand_bytes(void *out, int len)
{
    static int fd = 0;
    
    if(!out)
        out = malloc(len);
    
    if(!fd) {
        fd = open("/dev/urandom", O_RDONLY);
    }
    read(fd, out, len);
    return out;
}

int myrand()
{
    int i;
    get_rand_bytes(&i, sizeof(i));
    return i;
}

/* well, give me a plain token, or nothing */
int validate_token(token *tok, const char *pin)
{
    int ret = 1;
    if(tok->magic != MAGIC) {
        card_setstrerror(_("This card is not yet initialized."));
        ret = 0;
        goto hell;
    }

    if(tok->magic1 != MAGIC) { /* token was encrypted, need PIN */
        if(!pin) {
            ret = VALIDATE_TOKEN_NEEDPIN;
            return ret;
        }
        
        aes_decrypt(&(tok->magic1), sizeof(*tok) - sizeof(tok->magic), &(tok->magic1), pin);

        if(tok->magic1 != MAGIC) {
            card_setstrerror(_("The PIN you provided is incorrect."));
            ret = 0;
            goto hell;
        }        
    
    } 
    
    return 1;

    hell:
    memset(tok, 0, sizeof(*tok));
    return ret;
}

static char last_error[150];
void card_setstrerror(const char *s)
{
    strcpy(last_error, s);
}

const char *card_strerror()
{
    return last_error;
}

/* vim: set fdm=syntax: */
