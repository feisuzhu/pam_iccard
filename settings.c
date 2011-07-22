#include <string.h>
#include <stdio.h>

#include <libintl.h>
#define _ gettext

#include "settings.h"
#include "common.h"

static int nsettings = 0;
static struct {
    char option[100];
    char value[100];
} settings[30];


static void add(char *in, int slot)
{
    char *s;
    s = strchr(in, '=');
    if(s) {
        strcpy(settings[slot].value, s+1);
        strncpy(settings[slot].option, in, s-in);
    } else {
        strcpy(settings[slot].option, in);
    }
}

void parse_args(int argc, char **argv, int preserve)
{
    if(!preserve) {
        nsettings = 0;
        memset(settings, 0, sizeof(settings));
    }
    while(argc--) {
        add(argv[argc], nsettings++);
    }
}

int parse_file(const char *fn, int preserve)
{
    FILE *f;
    char buf[1000];

    if(!preserve) {
        nsettings = 0;
        memset(settings, 0, sizeof(settings));
    }
    
    f = fopen(fn, "r");
    if(!f) {
        sprintf(buf, _("parse file: can't open %s"), fn);
        card_setstrerror(buf);
        return 0;
    }
    while(fgets(buf, sizeof(buf), f)) {
        if(buf[0] != '#') {
            buf[strlen(buf)-1] = 0;
            add(buf, nsettings++);
        }
    }
    fclose(f);
    return 1;
}

char *get_setting(const char *o)
{
    int i = nsettings;
    while(i--) {
        if(!strcmp(o, settings[i].option)) {
            return settings[i].value;
        }
    }

    return NULL;
}
