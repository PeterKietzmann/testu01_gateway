extern "C" {
    #include <ulcg.h>
    #include <unif01.h>
    #include <bbattery.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern "C" unsigned long ProxyRng_getUInt32(void *param, void *state);

extern "C" double ProxyRng_getDouble01(void *param, void *state);

/**
 * Received uint32_t random number on stdin encoded in hexa with exactly 8 hexa digit per number and no spacing between numbers.
 */
struct ProxyRng : unif01_Gen
{
    ProxyRng( const std::string &name, bool shouldReverseBits)
    {
        this->nameStr = name;
        this->name = nameStr.c_str();
        this->shouldReverseBits = shouldReverseBits;
        this->GetBits = &ProxyRng_getUInt32;
        this->GetU01 = &ProxyRng_getDouble01;
        this->param = this;
        this->state = this;
    }
    
    uint32_t getUInt32();
    

    static const size_t bufferSize = 256;
    size_t receivedIndex = bufferSize;
    bool shouldReverseBits;
    uint32_t received[bufferSize];
    std::string nameStr;
};



static const uint8_t hexaTable[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

static inline uint32_t digitFromHexa(char c)
{
    return hexaTable[(c - '0') & ~0x20]; // The & ~0x20 force letter to upper case
}

static inline uint32_t reverseBits(uint32_t n) {
    n = (n >> 1) & 0x55555555 | (n << 1) & 0xaaaaaaaa;
    n = (n >> 2) & 0x33333333 | (n << 2) & 0xcccccccc;
    n = (n >> 4) & 0x0f0f0f0f | (n << 4) & 0xf0f0f0f0;
    n = (n >> 8) & 0x00ff00ff | (n << 8) & 0xff00ff00;
    n = (n >> 16) & 0x0000ffff | (n << 16) & 0xffff0000;
    return n;
}

static uint32_t decodeUInt32(char *digits)
{
    uint32_t v = 0;
    for ( size_t i=0; i < 8; ++i)
    {
        v = (v << 4) | digitFromHexa(digits[i]);
    }
    return v;
}

inline uint32_t ProxyRng::getUInt32()
{
    if ( receivedIndex == bufferSize ) // refill the buffer
    {
        static char fileBuffer[bufferSize * 8];
        char *read = fgets(fileBuffer, sizeof(fileBuffer), stdin);
        if ( read == nullptr)
        {
            fprintf(stdout, "Fatal error: failed to read random number from stdin...");
            abort();
        }
        for ( size_t index=0; index < bufferSize; ++index )
        {
            received[index] = decodeUInt32(read + 8*index);
        }
        receivedIndex = 0;
    }
    if ( shouldReverseBits )
    {
        return reverseBits(received[receivedIndex]);
    }
    return received[receivedIndex];
}



extern "C" unsigned long ProxyRng_getUInt32(void *param, void *state)
{
    return static_cast<ProxyRng *>(state)->getUInt32();
}

extern "C" double ProxyRng_getDouble01(void *param, void *state)
{
    return static_cast<ProxyRng *>(state)->getUInt32() * unif01_INV32;
}


typedef void (*RunnerFn)(unif01_Gen * gen);
    
    
int main (int argc, const char **argv )
{
    ++argv; --argc;

    ProxyRng rng = {};
    RunnerFn runner = &bbattery_Crush;
    std::string name = "UnknownRng";
    bool reverseBits = false;
    while ( argc > 0 )
    {
        std::string arg = *argv++;
        --argc;
        if ( arg == "--small-crush" ||  arg == "-s" )
        {
            runner = &bbattery_SmallCrush;
        }
        else if ( arg == "--crush"  ||  arg == "-c" )
        {
            runner = &bbattery_Crush;
        }
        else if ( arg == "--big-crush"  ||  arg == "-b" )
        {
            runner = &bbattery_BigCrush;
        }
        else if ( arg == "--name" )
        {
            if ( argc == 0 )
            {
                printf("Expected name after --name.")
                exit(2);
            }
            name = *argv++;
            --argc;
        }
        else if ( arg == "--reverse-bits" ||  arg == "-r" )
        {
            reverseBits = true;
        }
        else if ( arg == "-h"  ||  arg == "--help" )
        {
            printf(R"HELP(
TestU01_gateway simulate a random number generator by reading generated
number from stdin.

Each generated 32 bits number must be serialized to stdin as 4 bytes.

Makes sure to run tests twice: once with and once without --reverse-bits.
            
Options:
--name: set the name of the random number generator (used in final report)
-s, --small-crush  : run SmallCrunch tests
-c, --crush        : run Crunch tests
-b, --big-crush    : run BigCrunch tests
-r, --reverse-bits : reverse bits of the decoded 32 bits integer (bit 0 is 
                     swapped with bit 31, bit 1 with bit 30...).
            )HELP");
        }
    }
}

