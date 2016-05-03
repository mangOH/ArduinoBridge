#include <arpa/inet.h>
#include "swi_mangoh_bridge_packet.h"

unsigned short swi_mangoh_bridge_packet_crcUpdate(unsigned short crc, const unsigned char* data, unsigned int len)
{
    unsigned char* ptr = (unsigned char*)data;
    unsigned int idx = 0;

    LE_ASSERT(data);

    for (idx = 0; idx < len; idx++)
    {
        unsigned char val = *ptr;
        val ^= crc & 0xFF;
        val ^= val << 4;

        crc = ((((unsigned short)val << 8) | ((crc >> 8) & 0xFF)) ^ (unsigned char)(val >> 4) ^ ((unsigned short)val << 3));
        ptr++;
    }

    return crc;
}

void swi_mangoh_bridge_packet_initResponse(swi_mangoh_bridge_packet_t* packet, uint32_t len)
{
    LE_ASSERT(packet);

    packet->msg.start = SWI_MANGOH_BRIDGE_PACKET_START;
    packet->msg.crc = SWI_MANGOH_BRIDGE_PACKET_CRC_RESET;
    packet->msg.len = htons(len);

    packet->msg.crc = swi_mangoh_bridge_packet_crcUpdate(packet->msg.crc, &packet->msg.start, sizeof(packet->msg.start));
    LE_DEBUG("message index(%u)", packet->msg.idx);
    packet->msg.crc = swi_mangoh_bridge_packet_crcUpdate(packet->msg.crc, &packet->msg.idx, sizeof(packet->msg.idx));

    LE_DEBUG("payload length(%u)", len);
    packet->msg.crc = swi_mangoh_bridge_packet_crcUpdate(packet->msg.crc, (unsigned char*)&packet->msg.len, sizeof(packet->msg.len));

    packet->msg.crc = len ? swi_mangoh_bridge_packet_crcUpdate(packet->msg.crc, packet->msg.data, len):packet->msg.crc;
    packet->msg.crc = htons(packet->msg.crc);
}

void swi_mangoh_bridge_packet_dumpBuffer(const unsigned char* buff, unsigned int len)
{
    unsigned char* ptr = (unsigned char*)buff;
    unsigned int idx = 0;

    LE_ASSERT(buff);

    for (idx = 0; idx < len;)
    {
        if ((len - idx) >= 16)
        {
            LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14], ptr[15]);
            idx += 16;
            ptr += 16;
        }
        else
        {
            switch(len - idx)
            {
            case 15:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14]);
                break;

            case 14:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8], ptr[9], ptr[10], ptr[11], ptr[12], ptr[13]);
                break;

            case 13:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8], ptr[9], ptr[10], ptr[11], ptr[12]);
                break;

            case 12:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8], ptr[9], ptr[10], ptr[11]);
                break;

            case 11:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8], ptr[9], ptr[10]);
                break;

            case 10:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8], ptr[9]);
                break;

            case 9:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x 0x%02x",
                    ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7],
                    ptr[8]);
                break;

            case 8:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x%02x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7]);
                break;

            case 7:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x%02x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6]);
                break;

            case 6:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x%02x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5]);
                break;

            case 5:
                LE_DEBUG("0x%02x%02x%02x%02x 0x%02x", ptr[0], ptr[1], ptr[2], ptr[3], ptr[4]);
                break;

            case 4:
                LE_DEBUG("0x%02x%02x%02x%02x", ptr[0], ptr[1], ptr[2], ptr[3]);
                break;

            case 3:
                LE_DEBUG("0x%02x%02x%02x", ptr[0], ptr[1], ptr[2]);
                break;

            case 2:
                LE_DEBUG("0x%02x%02x", ptr[0], ptr[1]);
                break;

            case 1:
                LE_DEBUG("0x%02x", ptr[0]);
                break;

            default:
                break;
            }

            idx = len;
        }
    }
}
