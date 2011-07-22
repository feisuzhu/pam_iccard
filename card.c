/* Routines for reading/writing SIM card
 * Token is stored at the last SMS record by default
 * Author: Proton
 * E-mail: feisuzhu@163.com
 * File created: 2011-2-27 20:51
 * License: BSD
 ************************************
 * Accept parameters:
 * reader=Reader_Name
 * sms_record=-1
 *
 * Reader_Name will indicate reader name "Reader Name" ( _ replaced with space )
 * sms_record means which record you want the token be stored,
 * 0 for the first, 1 for the second, -1 for the last, -2 for the 2nd in reverse order
 */

#include <stdlib.h>
#include <string.h>
#include <winscard.h>

#include <libintl.h>

#define _ gettext

#include "common.h"
#include "settings.h"
#include "card.h"

#define LEN sizeof(token)

typedef struct {
    WORD len;
    BYTE sw1;
    BYTE sw2;
} __attribute__((packed)) response;

static __attribute__((pure)) WORD bswap(WORD w)
{
    return ((w << 8) & 0xFF00) | ((w >> 8) & 0xFF);
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

#define FID_MF 0x3F00
#define     FID_DF_TELECOM 0x7F10
#define         FID_EF_SMS 0x6F3C
#define         FID_EF_ADN 0x6F3A
#define     FID_DF_GSM 0x7F20
#define         FID_EF_IMSI 0x6F07

static response select_file(SCARDHANDLE hCard, WORD fileid)
{
    BYTE apdu[] = { 0xA0, 0xA4, 0x00, 0x00, 0x02, /* >> */ 0x00, 0x00 /* << file id */ };
    BYTE buf[4];

    apdu[5] = (fileid>>8) & 0xFF;
    apdu[6] = fileid & 0xFF;
    return trans(hCard, apdu, sizeof(apdu), buf, sizeof(buf));
}

static response get_response(SCARDHANDLE hCard, BYTE *out, DWORD len)
{
    BYTE apdu[] = { 0xA0, 0xC0, 0x00, 0x00, 0x00 /* size */ };
    apdu[4] = (BYTE)len;
    return trans(hCard, apdu, sizeof(apdu), out, len+2);
}

static response read_record_abs(SCARDHANDLE hCard, DWORD index, BYTE *out, DWORD rlen)
{
    BYTE apdu[] = { 0xA0, 0xB2, 0x00 /* index */, 0x04, 0x00 /* record len */ };
    apdu[2] = (BYTE)index;
    apdu[4] = (BYTE)rlen;
    return trans(hCard, apdu, sizeof(apdu), out, rlen+2);
}

static response write_record_abs(SCARDHANDLE hCard, DWORD index, const BYTE *in, DWORD len)
{
    BYTE oapdu[] = {0xA0, 0xDC, 0x00 /* index */, 0x04, 0x00 /* size */ /*, followed by [size] bytes */ };
    BYTE apdu[sizeof(oapdu) + len];
    BYTE buf[4];

    memcpy(apdu, oapdu, sizeof(oapdu));
    apdu[2] = (BYTE)index;
    apdu[4] = (BYTE)len;

    memcpy(&apdu[5], in, len);
    return trans(hCard, apdu, sizeof(apdu), buf, sizeof(buf));
}


#define fail_if(exp, errmsg) \
    if(exp) { \
        char *s = (errmsg); \
        ret = 0; \
        if(s) \
            err = s; \
        goto finish; \
    }

static int inline trans_succeeded(response r)
{
    return r.sw1 == 0x90 || r.sw1 == 0x9F || r.sw1 == 0x91;
}

typedef struct {
    WORD rfu1;
    WORD size;
    WORD fid;
    BYTE ftype;
    BYTE rfu2[5];
    BYTE cb;
    BYTE gsmdata[0];
} __attribute__((packed)) select_resp;

static char *repl(char *s) // just replace '_' with ' '
{
    char *o = s;
    if(!s) return NULL;
    while(*s) {
        if(*s == '_') *s = ' ';
        s++;
    }
    return o;
}

static int token_manipulate(token *ptok, int op)
{
    //TSUNDERELLA(0);
    SCARDCONTEXT ctx = 0; // well, this is just a long
    SCARDHANDLE hCard = 0; // the same
    char *reader = NULL; // SCR
    char *err = NULL;
    DWORD dwActiveProtocol, nRec, dwRecSize;
    response resp;
    BYTE buf[300];
    int ret = 1;
    int status;

    
    reader = repl(get_setting("reader"));
    
    fail_if(!reader, _("** PLEASE SPECIFY CARD READER NAME IN CONFIG FILE OR PAM PARAMETERS **"));

    fail_if(SCARD_S_SUCCESS != SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &ctx), _("SCardEstablishContext failed"));
    
    status = SCardConnect(
        ctx,
        reader,
        SCARD_SHARE_EXCLUSIVE,
        SCARD_PROTOCOL_T0,
        &hCard,
        &dwActiveProtocol
    );
    
    fail_if(status == SCARD_E_NO_SMARTCARD, _("No smartcard is currently in reader."));
    fail_if(status != SCARD_S_SUCCESS, _("SCardConnect failed"));

    fail_if(!trans_succeeded(select_file(hCard, FID_MF)), _("APDU failure"));
    fail_if(!trans_succeeded(select_file(hCard, FID_DF_TELECOM)), _("APDU failure"));
    
    resp = select_file(hCard, FID_EF_SMS);
    fail_if(!trans_succeeded(resp), _("APDU failure"));

    resp = get_response(hCard, buf, resp.sw2); // sw2 stores the length of response of the previous select instruction generated
    fail_if(!trans_succeeded(resp), _("APDU failure"));

    dwRecSize = ((select_resp *)buf)->gsmdata[1]; // well, RTFM(GSM 11.11) if you don't understand
    nRec = bswap(((select_resp *)buf)->size) / dwRecSize;

    fail_if(dwRecSize < LEN + 4, _("Record on card too short to fit in auth token")); // token begins at 4th byte
    
    char *s;
    int loc;
    if((s = get_setting("sms_record"))) {
        int x = nRec;
        loc = atoi(s);
        while(loc < 0) {
            loc += x;
            x <<= 1;
        }
        loc %= nRec;
        loc++;
    }


    switch(op) {
    case 1: // read
        fail_if(!trans_succeeded(read_record_abs(hCard, loc, buf, dwRecSize)), _("APDU failure"));
        memcpy(ptok, &buf[4], LEN);
        break;

    case 2: // write
        memset(buf, 0xFF, sizeof(buf));
        buf[0] = 0;
        memcpy(&buf[4], ptok, LEN);
        fail_if(!trans_succeeded(write_record_abs(hCard, loc, buf, dwRecSize)), _("APDU failure"));
        break;

    default:
        break;
    }

    finish:
    if(hCard) SCardDisconnect(hCard, SCARD_RESET_CARD);
    if(ctx) SCardReleaseContext(ctx);
    if(err) card_setstrerror(err);
    
    return ret;
}

int read_token(token *out)
{
    return token_manipulate(out, 1);
}

int write_token(token *in)
{
    return token_manipulate(in, 2);
}

// vim: fdm=syntax:
