#include <stdio.h>
#include <winscard.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    WORD len;
    BYTE sw1;
    BYTE sw2;
} __attribute__((packed)) response;

char *hex(char *in, int len, char *out)
{
    const char *a = "0123456789ABCDEF"; /* for convenience */
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

static char conv(char c)
{
    switch(c) {
    case '0' ... '9':
        c -= '0';
        break;

    case 'A' ... 'F':
        c -= 'A' - 10;
        break;

    case 'a' ... 'f':
        c -= 'a' - 10;
        break;

    default:
        printf("Invalid char '%c'\n", c);
        c = 0;
    }
    return c;
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
        out[i] = (conv(in[i<<1]) << 4) | (conv(in[(i<<1)+1]));
    }
    return out;
}


static response trans(SCARDHANDLE hCard, BYTE *in, int len, BYTE *out, DWORD bufsize)
{
    LONG ret;
    SCARD_IO_REQUEST ioreq;
    response r = { -1, 0, 0};

    ret = SCardTransmit(hCard, SCARD_PCI_T0, in, len, &ioreq, out, &bufsize);
    if(ret == SCARD_S_SUCCESS) {
        r.len = bufsize-2;
        r.sw1 = out[bufsize-2];
        r.sw2 = out[bufsize-1];
    }
    return r;
}


void do_dump(SCARDHANDLE hCard)
{
    BYTE buf[100];
    BYTE apdu[100];
    BYTE sw[100];
    int l;
    
    while(1) {
        printf("APDU> ");
        fgets(buf, sizeof buf, stdin);
        l = strlen(buf)-1;
        buf[l] = 0;
        dehex(buf, l, apdu);
        l = trans(hCard, apdu, l>>1, sw, sizeof sw).len;
        hex(sw, l+2, buf);
        printf("%s\n", buf);
    }
}

int main()
{
    SCARDCONTEXT ctx;
    LONG ret;
    SCARDHANDLE hCard;
    DWORD dwActiveProtocol;
    
    ret = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &ctx);
    ret = SCardConnect(ctx,
        "O2 Micro Oz776 00 00",
        SCARD_SHARE_EXCLUSIVE,
        SCARD_PROTOCOL_T0,
        &hCard,
        &dwActiveProtocol
    );
    do_dump(hCard);
    ret = SCardDisconnect(hCard, SCARD_RESET_CARD);
    SCardReleaseContext(ctx);
    return 0;
}
