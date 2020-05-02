// WinJudge.cpp : �������̨Ӧ�ó������ڵ㡣
//
// ���ٳ���,��Ҫ��¼����ִ��ռ�õ��ڴ�����е�ʱ��
// 
//

#include "stdafx.h"
#include<string>
#include<windows.h>
#include<winerror.h>
#include<Psapi.h>
#include<iostream>

using namespace std;

#define AC 0
#define PE 1
#define TLE 2
#define MLE 3
#define WA 4
#define RE 5
#define OLE 6
#define CE 7
#define SE 8

#define DEBUG false

struct result {
	int status;
	int timeUsed;
	int memoryUsed;
};

__int64 processStartTime = 0;

std::wstring s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}
LPWSTR ConvertToLPWSTR(const std::string& s)
{
	LPWSTR ws = new wchar_t[s.size() + 1]; // +1 for zero at the end
	copy(s.begin(), s.end(), ws);
	ws[s.size()] = 0; // zero at the end
	return ws;
}

/*
monitor the user process
*/
void monitor(PROCESS_INFORMATION pi, int timeLimit, int memoryLimit, struct result *rest) {
	// �ȴ�����ִ�����
	DWORD waitStatus;
	waitStatus = WaitForSingleObject(pi.hProcess, timeLimit);
	switch (waitStatus)
	{
	case WAIT_ABANDONED:
		if (DEBUG) {
			cout << "waiteStatus:" << waitStatus << endl;
		}
		rest->status = RE;
		return;
	case WAIT_TIMEOUT:
		rest->status = TLE;
		// ��ʱǿ���˳�,������ѭ��
		if (DEBUG) {
			cout << "waiteStatus: "<<"TIMEOUT" << waitStatus << endl;
		}
		TerminateProcess(pi.hProcess, 1);
		return;
	case WAIT_OBJECT_0:
		break;
	default:
		return;
	}
	// ��ý��̵���Ϣ
	PROCESS_MEMORY_COUNTERS pmc;
	if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc)))
	{
		// ʱ�� ����ʱ��ռ��
		LARGE_INTEGER large_integer;
		QueryPerformanceCounter(&large_integer);
		__int64 processEndTime = large_integer.QuadPart;
		QueryPerformanceFrequency(&large_integer);
		double div = 1.0;
		div = large_integer.QuadPart;
		// ת��Ϊms
		rest->timeUsed = (processEndTime - processStartTime) * 1000 / div;
		// �����ڴ�ռ��
		long memory_peak_use = pmc.PeakWorkingSetSize;
		rest->memoryUsed = (memory_peak_use / 1024);
		// ����˳���״̬��
		DWORD exitCode;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		// �ⲿ������������ returnΪ0
		if (exitCode != 0) {
			// �鿴�������
			DWORD last_error = GetLastError();
			switch (last_error)
			{
			case ERROR_INVALID_SEGMENT_NUMBER:
				if (rest->memoryUsed > memoryLimit) {
					rest->status = MLE;
				}
				else {
					if (DEBUG) {
						cout << "��Ч��:exitCode:" << exitCode << endl;
					}
					rest->status = RE;
				}
				break;
			case ERROR_SEM_TIMEOUT:
				rest->status = TLE;
				break;
			case ERROR_STACK_OVERFLOW:
				rest->status = MLE;
				if (DEBUG)
				{
					cout << "ջ���" << endl;
				}
				break;
			case 0xC00000FD:
				rest->status = MLE;
				if (DEBUG) {
					cout << "ջ���" << endl;
				}
				break;
			default:
				if (int(exitCode) == int(3221225725))
				{
					if (DEBUG) {
						cout << "stack overflow: exitCode:" << hex << exitCode << endl;
					}
					rest->status = MLE;
				}
				else {
					if (DEBUG) {
						cout << "other problem: exitCode:" <<hex<< exitCode << endl;
					}
					rest->status = RE;
				} 
				break;
			}
		}
		else {
			// �����˳�
			if (rest->timeUsed > timeLimit)
			{
				// ��ʱ
				rest->status = TLE;
				TerminateProcess(pi.hProcess, 1);
			}
			else if (rest->memoryUsed > memoryLimit) {
				// ���ڴ�
				rest->status = MLE;
				TerminateProcess(pi.hProcess, 1);
			}
			else {
				// ����ִ�����
				rest->status = AC;
			}
		}
	}
	else {
		// �޷�����ڴ���Ϣ,����RE
		if (DEBUG)
		{
			cout << "�޷�����ڴ���Ϣ" << endl;
		}
		rest->status = RE;
	}
}

void runProccess(string cmd, int timeLimit, int memoryLimit, string inputPath, string outputPath) {
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	// �ӽ��̵�����ܵ����
	SECURITY_ATTRIBUTES saAttr;

	// ��������ļ��ľ��
	// ��ȡ�ļ��ľ��
	HANDLE fileReadHandle = NULL;
	// д���ļ��ľ��
	HANDLE fileWriteHandle = NULL;

	memset(&saAttr, 0, sizeof(saAttr));
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	std::wstring ipath = s2ws(inputPath);
	wstring opath = s2ws(outputPath);
	LPCWSTR input_path = ipath.c_str();
	LPCWSTR output_path = opath.c_str();
	LPWSTR run_cmd = ConvertToLPWSTR(cmd);

	// LPCWSTR input_path = L"D:\\C\\TestInputOutput\\Debug\\input.txt";
	// LPCWSTR output_path = L"D:\\C\\TestInputOutput\\Debug\\output.txt";
	// LPWSTR run_cmd = ConvertToLPWSTR("python D:\\GraduationProject\\WinJudge\\Debug\\test.py");

	// ���ö�ȡ�ļ����ݵľ��
	fileReadHandle = (HANDLE)CreateFile(input_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &saAttr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	// ���д���ļ��ľ��
	fileWriteHandle = (HANDLE)CreateFile(output_path, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, &saAttr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	
	if (NULL == fileReadHandle)
	{
		printf("Can't open input file\n");
		return;
	}
	if (fileWriteHandle == NULL) {
		printf("Can't open output file \n");
	}
	SetFilePointer(fileWriteHandle, NULL, NULL, FILE_BEGIN);
	memset(&si, 0, sizeof(si));
	// si.cb = sizeof(si);
	si.cb = memoryLimit * 1024;
	si.hStdError = fileWriteHandle;
	si.hStdOutput = fileWriteHandle;
	si.hStdInput = fileReadHandle;
	si.dwFlags |= STARTF_USESTDHANDLES;
	ZeroMemory(&pi, sizeof(pi));
	// Start the child process. 
	if (!CreateProcess(NULL, run_cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, 0, &si, &pi))
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
		return;
	}
	// ��ʼ��ʱ
	LARGE_INTEGER large_integer;
	QueryPerformanceCounter(&large_integer);
	processStartTime = large_integer.QuadPart;
	struct result rest;
	rest.status = RE;
	rest.timeUsed = -1;
	rest.memoryUsed = -1;
	CloseHandle(fileWriteHandle);
	CloseHandle(fileReadHandle);
	// ��ؽ���
	monitor(pi, timeLimit, memoryLimit, &rest);
	printf("{\"status\":%d,\"timeUsed\":%d,\"memoryUsed\":%d}", rest.status, rest.timeUsed, rest.memoryUsed);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

int main(int argc, char ** argv)
{
	if (argc == 6)
	{
		string cmd(argv[1]);
		int replaceIndex = cmd.find("@");
		while(replaceIndex != -1)
		{
			cmd.replace(replaceIndex, 1, " ");
			replaceIndex = cmd.find("@");
		}
		int timeLimit = NULL;
		int memoryLimit = NULL;
		timeLimit = atoi(argv[2]);
		memoryLimit = atoi(argv[3]);
		string inputPath(argv[4]);
		string outputPath(argv[5]);
		if (DEBUG)
		{
			cout << "====info====" << endl;
			cout << "cmd:" << cmd << endl;
			cout << "timeLimit" << timeLimit << endl;
			cout << "memoryLimit" << memoryLimit << endl;
			cout << "input: " << inputPath << endl;
			cout << "output:" << outputPath<<endl;
			cout << "===========" << endl;
		}
		runProccess(cmd, timeLimit, memoryLimit, inputPath, outputPath);
	}
	else {
		struct result rest;
		rest.status = SE;
		rest.memoryUsed = -1;
		rest.timeUsed = -1;
		printf("{\"status\":%d,\"timeUsed\":%d,\"memoryUsed\":%d}", rest.status, rest.timeUsed, rest.memoryUsed);
	}
	if (DEBUG) {
		// system("pause");
	}
	return 0;
}
