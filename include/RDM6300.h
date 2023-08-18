#pragma once

#include <string>

class RDM6300
{
public:
    /// Size of ID in bytes
    static const int ID_SIZE = 5;

    using Card_id = uint64_t;
    
    RDM6300()
    {
    }
    
    bool add_byte(uint8_t input)
    {
        // Frame format:
        // <STX> <digit1> ... <digit10> <checksum1> <checksum2> <ETX>
#ifdef PROTOCOL_DEBUG
        printf("Add %d\n", (int) input);
#endif
        if (input == STX)
        {
            m_state = 1;
            m_checksum = 0;
            m_index = 0;
#ifdef PROTOCOL_DEBUG
            printf("STX: State 1\n");
#endif
        }
        else if (input == ETX)
        {
            const auto old_state = m_state;
            m_state = 0;
#ifdef PROTOCOL_DEBUG
            printf("ETX: State 0\n");
#endif
            if (old_state == 5)
                return true;
#ifdef PROTOCOL_DEBUG
            printf("Error: Old state %d\n", old_state);
#endif
        }
        else
        {
            switch (m_state)
            {
            case 1:
                // Store 1st digit
                m_temp = input - '0';
                if (m_temp > 9)
                    m_temp -= 7;
                m_state = 2;
#ifdef PROTOCOL_DEBUG
                printf("State 2: %d\n", m_temp);
#endif
                break;

            case 2:
                // Store one byte
                input -= '0';
                if (input > 9)
                    input -= 7;
                m_temp = (m_temp << 4) | input;
#ifdef PROTOCOL_DEBUG
                printf("Store %d at %d\n", m_temp, m_index);
#endif
                m_buf[m_index++] = m_temp;
                if (m_index >= ID_SIZE)
                {
                    m_state = 3; 
#ifdef PROTOCOL_DEBUG
                    printf("State 3\n");
#endif
                }
                else
                {
                    m_state = 1; 
#ifdef PROTOCOL_DEBUG
                    printf("State 1: %d\n", m_temp);
#endif
                }
                break;

            case 3:
                // Store checksum
                m_checksum = input - '0';
                if (m_checksum > 9)
                    m_checksum -= 7;
                m_state = 4;
#ifdef PROTOCOL_DEBUG
                printf("State 3: %d\n", m_state);
#endif
                break;

            case 4:
                // Store checksum
                input -= '0';
                if (input > 9)
                    input -= 7;
                m_checksum = (m_checksum << 4) | input;
                m_state = 5;
#ifdef PROTOCOL_DEBUG
                printf("State 4: %d\n", m_checksum);
#endif
            }
        }
        return false;
    }

    Card_id get_id() const
    {
        Card_id id = 0;
        int cs = 0;
        // Note that this assumes little-endianness
        auto p = reinterpret_cast<uint8_t*>(&id) + ID_SIZE;
        for (int i = 0; i < ID_SIZE; ++i)
        {
            --p;
            *p = m_buf[i];
            cs ^= m_buf[i];
        }
        if (cs != m_checksum)
        {
#ifdef PROTOCOL_DEBUG
            printf("CS error: Exp %d act %d", m_checksum, cs);
#endif
            return 0;
        }
        return id;
    }
    
private:
    static const char STX = 2;
    static const char ETX = 3;
    int m_state = 0;
    int m_checksum = 0;
    int m_temp = 0;
    int m_index = 0;
    unsigned char m_buf[ID_SIZE];
};
