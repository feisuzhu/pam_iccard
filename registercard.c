/* register card, make a card to be login credential of specific user
 * Author: Proton
 * E-mail: feisuzhu@163.com
 * File created: 2011-2-12 15:18
 * License: BSD
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#include <libintl.h>
#include <locale.h>

#define _ gettext

#include <pwd.h>

#include "common.h"
#include "card.h"
#include "settings.h"

int main(int argc, char **argv)
{
    struct passwd *pw;
    userinfo *uih, *u;
    uid_t uid;
    token tok;
    int old = 0;

    setlocale(LC_ALL, "");
    bindtextdomain("pam_iccard", ".");
    textdomain("pam_iccard");
    
    
    if(geteuid()) {
        printf(_("Won't work without effective root!\n"));
        exit(1);
    }

    if(argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        printf(
            _("Tool for register card, for pam_iccard\n"
              "Usage: registercard [username]\n")
        );
        return 0;
    }
    
    parse_file("/etc/pam_iccard.conf", 0);

    uid = getuid();
    
    if(argc>1) {
        pw = getpwnam(argv[1]);
    } else {
        pw = getpwuid(uid);
    }
    
    if(!pw) {
        printf(_("Unknown user %s\n"), argv[1]);
        exit(1);
    }

    if(uid && uid != pw->pw_uid) {
        printf(_("You are not authorized to register a card for %s\n"), pw->pw_name);
        exit(1);
    }
    
    uih = read_userinfo();

    if(!uih) {
        printf(_("Can't read userinfo!\n"));
        exit(1);
    }

    u = find_user_by_name(uih, pw->pw_name);
    if(u) {
        struct termios t1, t2;
        char c;
        
        tcgetattr(STDIN_FILENO, &t1);
        t2 = t1;
        t1.c_lflag &= ~(ICANON | ECHO);
        old = 1;
        tcsetattr(STDIN_FILENO, TCSANOW, &t1);

        printf(_("You(or the user you specified) has already registered a card, \n"
                 "re-registering will cause old card unusable,\n"));

        do {
            printf(_("Are you sure?(Y/N)"));
            c = tolower(getchar());
            printf("\n");
        } while(c != 'y' && c != 'n');

        tcsetattr(STDIN_FILENO, TCSANOW, &t2);
        if(c == 'n') {
            exit(1);
        }
    } else {
        u = malloc(sizeof(userinfo));
        memset(u, 0, sizeof(userinfo));
        strncpy(u->username, pw->pw_name, sizeof(u->username));
    }

    if(!read_token(&tok)) {
        printf(_("Read card failed: %s\n"), card_strerror());
        exit(1);
    }
    
    {
        char pin[100];
        int ret;
        
        ret = validate_token(&tok, NULL);
        if(ret == VALIDATE_TOKEN_NEEDPIN) {
            struct termios t1, t2;
            
            tcgetattr(STDIN_FILENO, &t1);
            t2 = t1;
            t1.c_lflag &= ~ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &t1);

            printf(_("PIN: "));
            fgets(pin, sizeof(pin), stdin);
            pin[strlen(pin)-1] = 0;

            printf("\n");
            ret = validate_token(&tok, pin);
            tcsetattr(STDIN_FILENO, TCSANOW, &t2);
        }
        
        if(!ret) {
            printf("%s\n", card_strerror());
            exit(1);
        }
    }

    u->id = tok.id;
    digest(&tok.magic1, u->id, u->hash);

    if(!old) {
        u->next = uih->next;
        uih->next = u; /* first one is a dummy */
    }
    
    if(!write_userinfo(uih)) {
        printf(_("Fxxk, can't write userinfo!\n"));
        exit(0);
    }

    printf(_("Registered.\n"));

    return 0;

}

/* vim: set fdm=syntax: */


