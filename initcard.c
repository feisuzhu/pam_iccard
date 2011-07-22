
/* Tool for initialize cards
 * Author: Proton
 * E-mail: feisuzhu@163.com
 * File created: 2011-2-10 10:06
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <termios.h>
#include <unistd.h>

#include <libintl.h>
#include <locale.h>

#define _ gettext

#include "common.h"
#include "card.h"
#include "settings.h"


int main(int argc, char **argv)
{
    token tok;
    
    setlocale(LC_ALL, "");
    bindtextdomain("pam_iccard", ".");
    textdomain("pam_iccard");

    if(argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
        printf(
            _("Tool for initialize card, for pam_iccard\n"
              "Usage: initcard\n")
        );
        return 0;
    }
    
    if(!parse_file("/etc/pam_iccard.conf", 0)) {
        printf(_("Error reading conf file: %s\n"), card_strerror());
        return 0;
    }

    if(!read_token(&tok)) {
        printf(_("Read card failed: %s\n"), card_strerror());
        goto hell;
    }

    if(tok.magic == MAGIC) {
        char c ;
        struct termios t1, t2;

        printf(
            _("It seems that this card has already been initialized,\n"
              "reinitializing will cause auth failure if the card already registered.\n")
        );
        
        tcgetattr(STDIN_FILENO, &t1);
        t2 = t1;
        t2.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &t2);
        do {
            printf(_("Are you sure?(Y/N)"));
            //fgets(str, sizeof(str), stdin);
            c=tolower(getchar());
            printf("\n");
        } while(!(c == 'y' || c == 'n'));
        tcsetattr(STDIN_FILENO, TCSANOW, &t1);

        if(c == 'n')
            goto hell;
    }

    tok.magic = MAGIC;
    tok.magic1 = MAGIC;
    (void) get_rand_bytes(&tok.id, sizeof(tok.id) + sizeof(tok.token));

    {
        char pin[100];
        struct termios t1, t2;
        
        tcgetattr(STDIN_FILENO, &t1);
        t2 = t1;
        t2.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &t2);
        printf(_("Enter your PIN: "));
        memset(pin, 0, sizeof(pin));
        fgets(pin, 100, stdin);
        pin[strlen(pin)-1] = 0;
        if(pin[0]) {
            aes_encrypt(&tok.magic1, sizeof(tok) - sizeof(tok.magic), &tok.magic1, pin);
        }
        printf("\n");
        tcsetattr(STDIN_FILENO, TCSANOW, &t1);
    }

    
    if(!write_token(&tok)) {
        printf(_("Write card failed: %s\n"), card_strerror());
        goto hell;
    }

    printf(_("Card initialized.\n"));

    hell:
    return 0;
}
