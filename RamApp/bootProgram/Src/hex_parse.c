#include "stm32f10x_usart.h"
#include "main.h"
#include "queue.h"
#include <stdio.h>

#define HEX_INIT_SEQUENCE           0x00U
#define HEX_RECEIVE_DATA            0x01U
#define HEX_DONE                    0x02U
#define HEX_FAIL                    0x03U
#define HEX_DEFAULT                 0xFFU

uint8_t Hex_state = HEX_INIT_SEQUENCE;
queueEntry *CurrentEntry;

typedef struct {
    uint8_t buff[MAX_QUEUE_ENTRY_SIZE];
    uint8_t length;
    uint8_t *add;
} hex_line;

uint32_t VectorTable_Addr = 0xFFFFFFFF;
uint32_t ResetHandler_Addr = 0xFFFFFFFF;
uint32_t Add_Addr;

/* receive hex data via uart and program it to ram */
void USART1_IRQHandler(void)
{
    if (Hex_state != HEX_RECEIVE_DATA)
    {
        (void)USART_ReceiveData(BOOT_UART); /* flush data */
        return;
    }
    
    if (CurrentEntry == 0)
    {
        CurrentEntry = getFreeQueueEntry();
        if (CurrentEntry == 0)
        {
            Hex_state = HEX_FAIL;
            (void)USART_ReceiveData(BOOT_UART); /* flush data */
            return;
        }
    }

    CurrentEntry->buff[CurrentEntry->pos] = (uint8_t)USART_ReceiveData(BOOT_UART);
    if (CurrentEntry->buff[CurrentEntry->pos] == '\n')
    {
        CurrentEntry->pos++;
        CurrentEntry = 0;   /* reset to next queue */
        putQueueEntry();
    }
    else
    {
        CurrentEntry->pos++;
        CurrentEntry->pos %= MAX_QUEUE_ENTRY_SIZE;
    }
}

void hex_transmitStr(char *s)
{
    while (*s != '\0')
    {
        USART_SendData(BOOT_UART, (uint16_t)(*s));
        while (USART_GetFlagStatus(BOOT_UART, USART_FLAG_TXE) != 1);
        s++;
    }
}

/*  check valid character
    return value:
    -1 - fail
    >=0 - success
 */
char validateChar(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    else if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* 
    Parse the data and fill in to out
    Return value: 
    0 - success
    < 0 - fail
*/
uint8_t isValidHex(hex_line *out, queueEntry *dat)
{
    uint8_t length = 0;
    uint8_t i, callength;
    uint8_t index = 0;
    uint16_t address;
    uint8_t checksum = 0, compchecksum;
    uint8_t type;
    char lownib, midlownib, midhighnib, highnib;

    for (i = 0; i < dat->pos; i++)
    {
        if (dat->buff[i] == '\r')
        {
            break;
        }
        length++;
    }

    /* minimum length */
    if (length < 11)
    {
        return -1;
    }

    /* First char */
    if (dat->buff[index] != ':')
    {
        return -2;
    }
    index++;

    /* two byte length */
    lownib = validateChar(dat->buff[index + 1]);
    highnib = validateChar(dat->buff[index]);
    if ((lownib == -1) || (highnib == -1))
    {
        return -3;
    }
    out->length = (((uint8_t)highnib << 4) | (uint8_t)lownib);
    callength = length - 11;
    if ((callength % 2) != 0 || (callength / 2) != out->length)
    {
        return -4;
    }
    checksum += out->length;
    index += 2;

    /* 4 byte address */
    highnib = validateChar(dat->buff[index]);
    midhighnib = validateChar(dat->buff[index + 1]);
    midlownib = validateChar(dat->buff[index + 2]);
    lownib = validateChar(dat->buff[index + 3]);
    if ((lownib == -1) || (highnib == -1) || (midhighnib == -1) || (midlownib == -1))
    {
        return -5;
    }
    address = (((uint16_t)highnib << 12) | ((uint16_t)midhighnib << 8)
                | ((uint16_t)midlownib << 4) | ((uint16_t)lownib));
    checksum += (uint8_t)(address >> 8);
    checksum += (uint8_t)address;
    index += 4;

    /* 2 byte type */
    highnib = validateChar(dat->buff[index]);
    lownib = validateChar(dat->buff[index + 1]);
    if ((highnib == -1) || (lownib == -1))
    {
        return -6;
    }
    type = (((uint8_t)highnib << 4) | (uint8_t)lownib);
    checksum += type;
    index += 2;

    if (type == 0)
    {
        /* data record */
        out->add = (uint8_t *)(Add_Addr + (uint32_t)address);
        if ((uint32_t)(out->add) < VectorTable_Addr)
        {
            VectorTable_Addr = (uint32_t)(out->add);
        }

        for (i = 0; i < out->length; i++)
        {
            highnib = validateChar(dat->buff[index]);
            lownib = validateChar(dat->buff[index + 1]);
            if ((highnib == -1) || (lownib == -1))
            {
                return -7;
            }
            out->buff[i] = (((uint8_t)highnib << 4) | (uint8_t)lownib);
            checksum += out->buff[i];
            index += 2;
        }
    }
    else if (type == 1)
    {
        /* end of record */
        if (out->length != 0 && address != 0)
        {
            return -8;
        }
        Hex_state = HEX_DONE;
    }
    else if (type == 2)
    {
        /* extended segment address record */
        if (out->length != 2 || address != 0)
        {
            return -9;
        }
        /* 4 byte address */
        highnib = validateChar(dat->buff[index]);
        midhighnib = validateChar(dat->buff[index + 1]);
        midlownib = validateChar(dat->buff[index + 2]);
        lownib = validateChar(dat->buff[index + 3]);
        if ((lownib == -1) || (highnib == -1) || (midhighnib == -1) || (midlownib == -1))
        {
            return -10;
        }
        Add_Addr = (((uint32_t)highnib << 16) | ((uint32_t)midhighnib << 12)
                | ((uint32_t)midlownib << 8) | ((uint32_t)lownib << 4));
        checksum += (((uint8_t)highnib << 4) | (uint8_t)midhighnib);
        checksum += (((uint8_t)midlownib << 4) | (uint8_t)lownib);
        index += 4;

        out->length = 0;
    }
    else if (type == 4)
    {
        /* extended linear address record */
        if (out->length != 2 || address != 0)
        {
            return -11;
        }
        /* 4 byte address */
        highnib = validateChar(dat->buff[index]);
        midhighnib = validateChar(dat->buff[index + 1]);
        midlownib = validateChar(dat->buff[index + 2]);
        lownib = validateChar(dat->buff[index + 3]);
        if ((lownib == -1) || (highnib == -1) || (midhighnib == -1) || (midlownib == -1))
        {
            return -12;
        }
        Add_Addr = (((uint32_t)highnib << 28) | ((uint32_t)midhighnib << 24)
                | ((uint32_t)midlownib << 20) | ((uint32_t)lownib << 16));
        checksum += (((uint8_t)highnib << 4) | (uint8_t)midhighnib);
        checksum += (((uint8_t)midlownib << 4) | (uint8_t)lownib);
        index += 4;

        out->length = 0;
    }
    else if (type == 5)
    {
        /* start address of reset handler */
        if (out->length != 4 || address != 0)
        {
            return -13;
        }

        /* 8 Byte address */
        for (i = 0; i < out->length; i++)
        {
            highnib = validateChar(dat->buff[index]);
            lownib = validateChar(dat->buff[index + 1]);
            if ((highnib == -1) || (lownib == -1))
            {
                return -14;
            }
            out->buff[3 - i] = (((uint8_t)highnib << 4) | (uint8_t)lownib);     /* change to low byte first format */
            checksum += out->buff[3 - i];
            index += 2;
        }
        ResetHandler_Addr = *((uint32_t *)&out->buff[0]);       /* low byte first parse */

        out->length = 0;
    }
    else
    {
        /* invalid type */
        return -15;
    }

    /* Two byte checksum */
    highnib = validateChar(dat->buff[index]);
    lownib = validateChar(dat->buff[index + 1]);
    if ((highnib == -1) || (lownib == -1))
    {
        return -16;
    }
    compchecksum = (((uint8_t)highnib << 4) | (uint8_t)lownib);
    checksum = (~checksum) + 1;
    if (checksum != compchecksum)
    {
        return -17;
    }

    return 0;
}

void Hex_receive(void)
{
    hex_line line;
    uint8_t i;
    void (*ptrResethandler)(void);
    char buff[100] = {0};

    switch (Hex_state)
    {
    case HEX_INIT_SEQUENCE:
        hex_transmitStr("Welcom to bootloader program\n");
        hex_transmitStr("Please load your hex file to the MCU\n");
        Hex_state = HEX_RECEIVE_DATA;
        break;

    case HEX_RECEIVE_DATA:
        if (getQeueuSize() > 0)
        {
            /* Parse the queue */
            if (isValidHex(&line, getreadQueueEntry()) == 0)
            {
                getQueueEntry();    /* decrease queue size */
                hex_transmitStr("Parse successful\n");

                /* Write to RAM */
                for (i = 0; i < line.length; i++)
                {
                    line.add[i] = line.buff[i];
                }
            }
            else
            {
                hex_transmitStr("Fail to parse Data\n");
                Hex_state = HEX_FAIL;
            }
        }
        break;

    case HEX_DONE:
        hex_transmitStr("Done\n");

        if (ResetHandler_Addr == 0xFFFFFFFF)
        {
            /* Read vector table */
            ResetHandler_Addr = *(((uint32_t *)VectorTable_Addr) + 1);
        }

        sprintf(buff, "Start the application at %lX address and vector table at %lX\n", 
            ResetHandler_Addr, VectorTable_Addr);
        hex_transmitStr(buff);

        ptrResethandler = (void (*)(void))ResetHandler_Addr;
        ptrResethandler();
        Hex_state = HEX_DEFAULT;
        break;
    
    case HEX_FAIL:
        hex_transmitStr("Exit the bootloader\n");
        Hex_state = HEX_DEFAULT;
        break;

    default:
        break;
    }
}