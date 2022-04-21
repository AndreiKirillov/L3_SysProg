// FileMapping.cpp: определяет процедуры инициализации для библиотеки DLL.
//

#include "pch.h"
#include "framework.h"
#include "FileMapping.h"
#include <string>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace std;
//
//TODO: если эта библиотека DLL динамически связана с библиотеками DLL MFC,
//		все функции, экспортированные из данной DLL-библиотеки, которые выполняют вызовы к
//		MFC, должны содержать макрос AFX_MANAGE_STATE в
//		самое начало функции.
//
//		Например:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// тело нормальной функции
//		}
//
//		Важно, чтобы данный макрос был представлен в каждой
//		функции до вызова MFC.  Это означает, что
//		должен стоять в качестве первого оператора в
//		функции и предшествовать даже любым объявлениям переменных объекта,
//		поскольку их конструкторы могут выполнять вызовы к MFC
//		DLL.
//
//		В Технических указаниях MFC 33 и 58 содержатся более
//		подробные сведения.
//

// CFileMappingApp

BEGIN_MESSAGE_MAP(CFileMappingApp, CWinApp)
END_MESSAGE_MAP()


// Создание CFileMappingApp

CFileMappingApp::CFileMappingApp()
{
	// TODO: добавьте код создания,
	// Размещает весь важный код инициализации в InitInstance
}


// Единственный объект CFileMappingApp

CFileMappingApp theApp;


// Инициализация CFileMappingApp

BOOL CFileMappingApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}

// структура для заголовка сообщения
struct header    
{
	int thread_id;
	int message_size;
};

HANDLE hWritePipe, hReadPipe;


extern "C"
{
	// Функция отправки сообщения через mapped файл
	__declspec(dllexport) bool __stdcall SendMappingMessage(void* message, header& h)
	{
		AFX_MANAGE_STATE(AfxGetStaticModuleState());
		HANDLE hFile = CreateFileA("myfile.dat", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)           // проверяем создание файла
			return false;

		HANDLE hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, h.message_size + sizeof(header), NULL);
		if (hFileMap == NULL)                   // проверяем создание файла, отображаемого в память
		{
			CloseHandle(hFile);
			return false;
		}

		char* buff = (char*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, h.message_size + sizeof(header));
		memcpy(buff, &h, sizeof(header));
		memcpy(buff + sizeof(header), message, h.message_size);

		UnmapViewOfFile(buff);
		CloseHandle(hFileMap);
		CloseHandle(hFile);
		return true;
	}

	__declspec(dllexport) bool __stdcall CreateProcessWithPipe(const char* process_name)
	{
		if (process_name == nullptr)
			return false;

		SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE };
		if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
			return false;
		if (SetHandleInformation(hWritePipe, HANDLE_FLAG_INHERIT, 0) == 0) // запрещаем наследование хэндла записи
			return false;                                               // дочерний процесс не сможет записывать в анонимный канал

		STARTUPINFO si{ 0 };
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = hReadPipe;
		si.hStdOutput = 0;
		si.hStdError = 0;

		PROCESS_INFORMATION pi;
		if (!CreateProcessA(NULL, (LPSTR)process_name, &sa, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
			return false;

		if (!(CloseHandle(pi.hThread) && CloseHandle(pi.hProcess)))
			return false;

		return true;
	}
}

__declspec(dllexport) header __stdcall ReadHeader()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	header h;
	h.thread_id = 0;
	h.message_size = 0;
	HANDLE hFile = CreateFileA("myfile.dat", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return h;                         // проверяем создание файла

	HANDLE hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, h.message_size + sizeof(header), NULL);
	if (hFileMap == NULL)                      // проверяем создание файла, отображаемого в память
	{
		CloseHandle(hFile);
		return h;
	}

	char* buff_for_header = (char*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, h.message_size + sizeof(header));

	memcpy(&h, buff_for_header, sizeof(header));
	UnmapViewOfFile(buff_for_header);
	CloseHandle(hFileMap);
	CloseHandle(hFile);

	return h;
}

__declspec(dllexport) string __stdcall ReadMessage(header& h)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	HANDLE hFile = CreateFileA("myfile.dat", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, 0);
	if (hFile == INVALID_HANDLE_VALUE)
		return string("");                         // проверяем создание файла

	HANDLE hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, sizeof(header), NULL);
	if (hFileMap == NULL)                      // проверяем создание файла, отображаемого в память
	{
		CloseHandle(hFile);
		return string("");
	}

	//// Читаем заголовок сообщения
	char* buff_for_header = (char*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(header));

	memcpy(&h, buff_for_header, sizeof(header));
	UnmapViewOfFile(buff_for_header);
	CloseHandle(hFileMap);

	hFileMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 0, h.message_size + sizeof(header), NULL);

	// читаем само сообщение
	char* buff_for_msg = (char*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(header) + h.message_size);

	char* message = new char[h.message_size];                        // память под сообщение
	memcpy(message, buff_for_msg + sizeof(header), h.message_size);
	string str_message(message);

	delete[] message;       // освобождаем память

	UnmapViewOfFile(buff_for_msg);
	CloseHandle(hFileMap);
	CloseHandle(hFile);
	return str_message;
}