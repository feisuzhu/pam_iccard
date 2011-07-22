/* PAM module for memory IC card based authenticate
 * Written by Proton
 * E-mail: feisuzhu@163.com
 * File created: 2011-2-6 14:46
 * License: BSD
 */

#include <stdarg.h>
#include <sys/types.h>

#include <locale.h>
#include <libintl.h>

#define _ gettext

#define PAM_SM_AUTH
//#define PAM_SM_PASSWORD

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

#include "common.h"
#include "card.h"
#include "settings.h"

#define AUTHTOK "Proton's Auth Token"

#define msg_error(args...) \
    if(!(flags & PAM_SILENT)) { \
        pam_prompt(pamh, PAM_ERROR_MSG, NULL, args); \
    }

#define msg_info(args...) \
    if(!(flags & PAM_SILENT)) { \
        pam_prompt(pamh, PAM_TEXT_INFO, NULL, args); \
    }

static void token_cleanup_callback(pam_handle_t *pamh, void *data, int error_status)
{
    if(data) {
        memset(data, 0, sizeof(token));
        free(data);
    }
}

PAM_EXTERN int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    int retval = PAM_AUTHINFO_UNAVAIL;
    token tok, *ptok;
    userinfo *uih = NULL, *u;
    const char *desired_user = NULL, *rhost = NULL;
    char hash[64];

    setlocale(LC_ALL, "");
    bindtextdomain("pam_iccard", ".");
    textdomain("pam_iccard");
    
    parse_file("/etc/pam_iccard.conf", 0);
    parse_args(argc, (char**)argv, 1);
    
    #if 0 /* Test code */ 
    #define hehe(x) \
        if(PAM_SUCCESS == pam_get_item(pamh, x, &s)) { \
            if(s) { \
                fprintf(f, #x " is %lX = %s\n", s, s); \
            } else { \
                fprintf(f, #x " is (null)\n"); \
            } \
        } else { \
            fprintf(f, "Getting " #x " failed.\n");\
        }

    {
        char *s;
        FILE *f;

        f = fopen("/dev/console", "w");
        hehe(PAM_USER);
        hehe(PAM_RUSER);
        hehe(PAM_RHOST);
        hehe(PAM_TTY);
        hehe(PAM_AUTHTOK);
        fclose(f);
    }
    #endif
    
    if(on("silent")) {
        flags |= PAM_SILENT;
    }
    
    if(PAM_SUCCESS == pam_get_item(pamh, PAM_RHOST, (const void **)&rhost)) {
        if(rhost && strlen(rhost)) {
            msg_error(_("pam_iccard doesn't support remote login."));
            retval = PAM_AUTH_ERR;
            goto last;
        }
    } else {
        msg_error(_("Can't get PAM_RHOST?!"));
        goto last;
    }
    
    if(on("noreading")) {
        if(PAM_SUCCESS != pam_get_data(pamh, AUTHTOK, (void *)&ptok)) {
            msg_error(_("'noreading' specified but no token was found."));
            goto last;
        }
        tok = *ptok;
    } else {
        if(!read_token(&tok)) {
            msg_error(_("Read card failed: %s"), card_strerror());
            goto last;
        }
    }

    /* Mission begins */

    retval = PAM_AUTH_ERR;

    {
        char *resp = NULL;
        int ret;

        ret = validate_token(&tok, NULL);
        if(ret == VALIDATE_TOKEN_NEEDPIN) {
            if(PAM_SUCCESS != pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &resp, _("PIN: "))) {
                msg_error(_("Failed to obtain PIN"));
            } else {
                ret = validate_token(&tok, resp);
            }
        }
        
        if(!ret) {
            msg_error("%s", card_strerror());
            goto last;
        }
    }
   
    if(PAM_SUCCESS != pam_get_item(pamh, PAM_USER, (const void **)&desired_user)) {
        msg_error(_("Can't get username ?!"));
        goto last;
    }

    uih = read_userinfo();

    if(!uih) {
        msg_error(_("Can't read userinfo!"));
        goto last;
    }

    if(desired_user) {
        u = find_user_by_name(uih, desired_user);
    } else {
        u = find_user_by_id(uih, tok.id);
    }

    if(!u) {
        retval = PAM_USER_UNKNOWN;
        msg_error(_("Unknown or unregistered user."));
        goto last;
    }
    
    (void) digest(&tok.magic1, u->id, hash);
    if(!memcmp(hash, u->hash, 64)) {
        retval = PAM_SUCCESS;
        pam_set_item(pamh, PAM_USER, u->username);

        if(on("storetoken")) {
            char *p;
            p = malloc(sizeof(token));
            *((token*)p) = tok;
            
            if(PAM_SUCCESS != pam_set_data(pamh, AUTHTOK, p, token_cleanup_callback)) {
                msg_error(_("pam_set_data failed?!"));
                goto last;
            }
        }
        
        goto last;
    } else {
        msg_error(_("Authenticate failure."));
        goto last;
    }
    
    last:
    memset(&tok, 0, sizeof tok);
    ptok = NULL;

    if(uih)
        free_userinfo(uih);

    return retval;
}

PAM_EXTERN int pam_sm_setcred (pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    /* Well, actually I got no clue what the hell it is :( */
    return PAM_SUCCESS;
}

#if 0

/* This function is untested, stale, and written without knowing what it is, do not use */

PAM_EXTERN int pam_sm_chauthtok(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
    int retval = PAM_AUTHTOK_ERR;
    const char *desired_user = NULL;
    token tok;
    userinfo *uih = NULL, *u;

    if(flags & PAM_PRELIM_CHECK) {
        char *s = NULL;
        
        D(("pam_sm_chauthtok: prelim check\n"));
        
        retval = PAM_TRY_AGAIN;
        if(PAM_SUCCESS != pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &s, "Insert your card, and press ENTER...\n")) {
            goto last;
        }
        if(s) free(s);
        
        if(open_card()) {
            retval = PAM_SUCCESS;
            goto last;
        } else {
            D(("pam_sm_chauthtok: prelim check: open card failed\n"));
            msg_error("Can't open card.\n");
            goto last;
        }

    } else if(flags & PAM_UPDATE_AUTHTOK) {
        D(("pam_sm_chauthtok: update authtok\n"));
        retval = PAM_AUTHTOK_ERR;
        
        // pam_get_item(pamh, PAM_USER, &desired_user);
        pam_get_user(pamh, &desired_user, "Username: ");
        
        if(!desired_user) {
            D(("pam_sm_chauthtok: can't get user\n"));
            retval = PAM_USER_UNKNOWN;
            goto last;
        }
        
        uih = read_userinfo();
        u = find_user_by_name(uih, desired_user);
        if(!u) {
            D(("pam_sm_chauthtok: unknown user"));
            retval = PAM_USER_UNKNOWN;
            goto last;
        }
        
        retval = PAM_AUTHTOK_RECOVERY_ERR;
        
        if(!open_card()) {
            D(("pam_sm_chauthtok: can't open card"));
            goto last;
        }

        if(!read_token(&tok)) {
            D(("pam_sm_chauthtok: can't read token"));
            goto last;
        }

        {
            char *s = NULL, *resp = NULL;
            
            s = validate_token(&tok, NULL);
            if(s == VALIDATE_TOKEN_NEEDPIN) {
                if(PAM_SUCCESS != pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &resp, "PIN: ")) {
                    s = "Can't get PIN!\n";
                } else {
                    s = validate_token(&tok, resp);
                }
            }
        
            if(s) {
                msg_error("%s", s);
                goto last;
            }
        }

        u->id = tok.id;
        u->salt = myrand();
        digest(&tok.magic1, u->salt, u->hash);
        write_userinfo(uih);

        retval = PAM_SUCCESS;
        goto last;
    }

    last:
    memset(&tok, 0, sizeof(tok));
    close_card();
    if(uih)
        free_userinfo(uih);
    return retval;
}
#endif
#if 0
int main()
{
    char *a = "Hello world!";

    int i = 100;
    
    while(i--) {
        printf("%s\n", hex(digest(a, 84065234, 0), 64, 0));
        fflush(stdout);
    }
    return 0;
}
#endif
#if 0
int main()
{
    char d[]  = "Proton rocks!!!\n";
    
    aes_encrypt(d, sizeof(d)-1, d, "test");
    aes_decrypt(d, sizeof(d)-1, d, "test");
    printf("%s", d);
    return 0;
}
#endif
/* vim: set fdm=syntax: */
