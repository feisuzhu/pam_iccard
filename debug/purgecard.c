#include <stdio.h>
#include <winscard.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    WORD rfu1;
    WORD size;
    WORD fid;
    BYTE ftype;
    BYTE rfu2[5];
    BYTE cb;
    BYTE gsmdata[0];
} __attribute__((packed)) select_resp;

WORD __attribute__((pure)) bswap(WORD w)
{
    return ((w << 8) & 0xFF00) | ((w >> 8) & 0xFF);
}

typedef struct {
    WORD len;
    BYTE sw1;
    BYTE sw2;
} __attribute__((packed)) response;

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

void do_dump(SCARDHANDLE hCard)
{
    BYTE select_mf[] = { 0xA0, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
    BYTE select_dftelecom[] = { 0xA0, 0xA4, 0x00, 0x00, 0x02, 0x7F, 0x10 };
    BYTE select_efsms[] = { 0xA0, 0xA4, 0x00, 0x00, 0x02, 0x6F, 0x3C };
    BYTE get_resp[] = { 0xA0, 0xC0, 0x00, 0x00, 0x00 /* size */ };
    BYTE read_record_abs[] = { 0xA0, 0xB2, 0x01 /* index */, 0x04, 0x00 /* size */ };
    DWORD cbRecv = 1000, nRec, sz, cbRec;
    SCARD_IO_REQUEST ioRecvPci;
    BYTE bRecvBuffer[1000];
    int i, fd;
    
    SCardTransmit(hCard, SCARD_PCI_T0, select_mf, sizeof(select_mf), &ioRecvPci, bRecvBuffer, &cbRecv);
    SCardTransmit(hCard, SCARD_PCI_T0, select_dftelecom, sizeof(select_dftelecom), &ioRecvPci, bRecvBuffer, &cbRecv);
    SCardTransmit(hCard, SCARD_PCI_T0, select_efsms, sizeof(select_efsms), &ioRecvPci, bRecvBuffer, &cbRecv);

    get_resp[4] = bRecvBuffer[cbRecv-1];

    cbRecv = 1000;
    SCardTransmit(hCard, SCARD_PCI_T0, get_resp, sizeof(get_resp), &ioRecvPci, bRecvBuffer, &cbRecv);

    #define P ((select_resp*)&bRecvBuffer)
    sz = bswap(P->size);
    cbRec = P->gsmdata[1];
    nRec = sz / cbRec;

    printf("size: %d\nfid: %04X\nrecsize: %d\nnRec: %d\n",
        sz,
        bswap(P->fid),
        cbRec,
        nRec
    );
    
    read_record_abs[4] = cbRec;

    #undef P

    memset(bRecvBuffer, 0xFF, sizeof(bRecvBuffer));
    for(i=1; i<=nRec; i++) {
        cbRecv = 1000;
        read_record_abs[2] = (BYTE)i;
        printf("Purge #%d...\n", i);
        write_record_abs(hCard, i, bRecvBuffer, cbRec);
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
