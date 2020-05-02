// WinJudge.cpp : 定义控制台应用程序的入口点。
//
// 跟踪程序,主要记录程序执行占用的内存和运行的时间
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
	// 等待进程执行完毕
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
		// 超时强制退出,避免死循环
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
	// 获得进程的信息
	PROCESS_MEMORY_COUNTERS pmc;
	if (GetProcessMemoryInfo(pi.hProcess, &pmc, sizeof(pmc)))
	{
		// 时间 计算时间占用
		LARGE_INTEGER large_integer;
		QueryPerformanceCounter(&large_integer);
		__int64 processEndTime = large_integer.QuadPart;
		QueryPerformanceFrequency(&large_integer);
		double div = 1.0;
		div = large_integer.QuadPart;
		// 转换为ms
		rest->timeUsed = (processEndTime - processStartTime) * 1000 / div;
		// 计算内存占用
		long memory_peak_use = pmc.PeakWorkingSetSize;
		rest->memoryUsed = (memory_peak_use / 1024);
		// 获得退出的状态码
		DWORD exitCode;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		// 外部进程正常结束 return为0
		if (exitCode != 0) {
			// 查看错误代码
			DWORD last_error = GetLastError();
			switch (last_error)
			{
			case ERROR_INVALID_SEGMENT_NUMBER:
				if (rest->memoryUsed > memoryLimit) {
					rest->status = MLE;
				}
				else {
					if (DEBUG) {
						cout << "无效段:exitCode:" << exitCode << endl;
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
					cout << "栈溢出" << endl;
				}
				break;
			case 0xC00000FD:
				rest->status = MLE;
				if (DEBUG) {
					cout << "栈溢出" << endl;
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
			// 正常退出
			if (rest->timeUsed > timeLimit)
			{
				// 超时
				rest->status = TLE;
				TerminateProcess(pi.hProcess, 1);
			}
			else if (rest->memoryUsed > memoryLimit) {
				// 超内存
				rest->status = MLE;
				TerminateProcess(pi.hProcess, 1);
			}
			else {
				// 正常执行完毕
				rest->status = AC;
			}
		}
	}
	else {
		// 无法获得内存信息,返回RE
		if (DEBUG)
		{
			cout << "无法获得内存信息" << endl;
		}
		rest->status = RE;
	}
}

void runProccess(string cmd, int timeLimit, int memoryLimit, string inputPath, string outputPath) {
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	// 子进程的输出管道句柄
	SECURITY_ATTRIBUTES saAttr;

	// 输入输出文件的句柄
	// 读取文件的句柄
	HANDLE fileReadHandle = NULL;
	// 写入文件的句柄
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

	// 设置读取文件内容的句柄
	fileReadHandle = (HANDLE)CreateFile(input_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &saAttr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	// 输出写入文件的句柄
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
	// 开始计时
	LARGE_INTEGER large_integer;
	QueryPerformanceCounter(&large_integer);
	processStartTime = large_integer.QuadPart;
	struct result rest;
	rest.status = RE;
	rest.timeUsed = -1;
	rest.memoryUsed = -1;
	CloseHandle(fileWriteHandle);
	CloseHandle(fileReadHandle);
	// 监控进程
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
