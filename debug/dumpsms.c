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

void do_dump(SCARDHANDLE hCard)
{
    BYTE select_mf[] = { 0xA0, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
    BYTE select_dftelecom[] = { 0xA0, 0xA4, 0x00, 0x00, 0x02, 0x7F, 0x10 };
    BYTE select_efsms[] = { 0xA0, 0xA4, 0x00, 0x00, 0x02, 0x6F, 0x3C };
    BYTE get_resp[] = { 0xA0, 0xC0, 0x00, 0x00, 0x00 /* size */ };
    BYTE read_record_abs[] = { 0xA0, 0xB2, 0x01 /* index */, 0x04, 0x00 /* size */ };
    DWORD cbRecv = 1000, nRec, sz;
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
    nRec = sz / P->gsmdata[1];

    printf("size: %d\nfid: %04X\nrecsize: %d\nnRec: %d\n",
        sz,
        bswap(P->fid),
        (DWORD)(P->gsmdata[1]),
        nRec
    );
    
    read_record_abs[4] = P->gsmdata[1];

    #undef P

    fd = open("/dev/shm/sms", O_WRONLY | O_CREAT, 0777);
    for(i=1; i<=nRec; i++) {
        cbRecv = 1000;
        read_record_abs[2] = (BYTE)i;
        printf("Getting %d SMS...\n", i);
        SCardTransmit(hCard, SCARD_PCI_T0, read_record_abs, sizeof(read_record_abs), &ioRecvPci, bRecvBuffer, &cbRecv);
        write(fd, bRecvBuffer, cbRecv-2);
    }
    close(fd);
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
