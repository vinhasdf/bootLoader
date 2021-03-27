#include "stm32f10x_usart.h"
#include "stm32f10x_flash.h"
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

/*
    return value:
    0 -> success
    1 -> busy
*/
uint8_t Hex_receive(void)
{
    hex_line line;
    uint8_t i;
    uint8_t retVal;
    uint32_t page_add;
    uint32_t app_size;
    FLASH_Status flash_status;
    char buff[100] = {0};

    switch (Hex_state)
    {
    case HEX_INIT_SEQUENCE:
        /* Erase all old application code from _APP_VECTOR_TABLE to _APP_VECTOR_TABLE + 52K */
        /* _APP_VECTOR_TABLE is align page */
        FLASH_Unlock();
        page_add = (uint32_t)&_APP_VECTOR_TABLE;
        app_size = (uint32_t)&_APP_ROM_SIZE;
        app_size /= 1024U;
        for (i = 0; i < app_size; i++)
        {
            flash_status = FLASH_ErasePage(page_add);
            if (flash_status != FLASH_COMPLETE)
            {
                /* Error to erase */
                Hex_state = HEX_FAIL;
                FLASH_Lock();
                hex_transmitStr("Erase program error\n");
                retVal = 1;
                return retVal;
            }
            page_add += 1024U;       /* increase 1K */
        }
        FLASH_Lock();
        // sprintf(buff, "%lX\n", app_size);
        // hex_transmitStr(buff);
        hex_transmitStr("Welcom to bootloader program\n");
        hex_transmitStr("Please load your hex file to the MCU\n");
        Hex_state = HEX_RECEIVE_DATA;
        retVal = 1;
        break;

    case HEX_RECEIVE_DATA:
        if (getQeueuSize() > 0)
        {
            /* Parse the queue */
            if (isValidHex(&line, getreadQueueEntry()) == 0)
            {
                getQueueEntry();    /* decrease queue size */
                /* Write to Flash */
                if (line.length > 0)
                {
                    FLASH_Unlock();
                    for (i = 0; (int8_t)i <= (int8_t)((int8_t)line.length - 4); i += 4)
                    {
                        flash_status = FLASH_ProgramWord((uint32_t)&line.add[i], 
                            *(uint32_t *)&line.buff[i]);
                        if (flash_status != FLASH_COMPLETE)
                        {
                            /* Error to erase */
                            Hex_state = HEX_FAIL;
                            FLASH_Lock();
                            sprintf(buff, "Program error: %d", flash_status);
                            hex_transmitStr(buff);
                            retVal = 1;
                            return retVal;
                        }
                    }
                    if ((line.length % 4) == 0)
                    {
                        line.buff[i + 1] = line.add[i + 1];     /* Save old value from i + 1 memory */
                        line.buff[i + 2] = line.add[i + 2];
                        line.buff[i + 3] = line.add[i + 3];
                        flash_status = FLASH_ProgramWord((uint32_t)&line.add[i], 
                            *(uint32_t *)&line.buff[i]);
                        if (flash_status != FLASH_COMPLETE)
                        {
                            /* Error to erase */
                            Hex_state = HEX_FAIL;
                            FLASH_Lock();
                            sprintf(buff, "Program error: %d", flash_status);
                            hex_transmitStr(buff);
                            retVal = 1;
                            return retVal;
                        }
                    }
                    FLASH_Lock();
                }
                hex_transmitStr("Parse successful\n");
            }
            else
            {
                hex_transmitStr("Fail to parse Data\n");
                Hex_state = HEX_FAIL;
            }
        }
        retVal = 1;
        break;

    case HEX_DONE:
        hex_transmitStr("Done\n");
        sprintf(buff, "Start the application at %lX address\n", 
            ResetHandler_Addr);
        hex_transmitStr(buff);
        Hex_state = HEX_DEFAULT;
        retVal = 0;
        break;
    
    case HEX_FAIL:
        hex_transmitStr("Exit the bootloader\n");
        Hex_state = HEX_DEFAULT;
        retVal = 1;
        break;

    default:
        retVal = 1;
        break;
    }

    return retVal;
}