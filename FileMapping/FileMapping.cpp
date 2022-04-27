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
	int event_code;
	int thread_id;
	int message_size;
};

HANDLE hWriteToChild, hChildRead;
HANDLE hChildWrite, hReadFromChild;


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

		// Канал для передачи сообщений дочернему процессу
		if (!CreatePipe(&hChildRead, &hWriteToChild, &sa, 0))
			return false;
		if (!SetHandleInformation(hWriteToChild, HANDLE_FLAG_INHERIT, 0)) // запрещаем наследование хэндла записи
		{                                                                 // дочерний процесс не сможет записывать в анонимный канал
			CloseHandle(hChildRead);
			CloseHandle(hWriteToChild);
			return false;                                               
		}

		// Канал для получения сообщений от дочернего процесса
		if (!CreatePipe(&hReadFromChild, &hChildWrite, &sa, 0))
		{
			CloseHandle(hChildRead);
			CloseHandle(hWriteToChild);
			return false;
		}
		if (!SetHandleInformation(hReadFromChild, HANDLE_FLAG_INHERIT, 0)) // запрещаем наследование хэндла чтения
		{                                                                  // дочерний процесс не сможет читать свои же сообщения
			CloseHandle(hChildRead);
			CloseHandle(hWriteToChild);
			CloseHandle(hReadFromChild);
			CloseHandle(hChildWrite);
			return false;                                               
		}

		STARTUPINFO si{ 0 };
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = hChildRead;
		si.hStdOutput = 0;//hChildWrite;
		si.hStdError = hChildWrite;

		PROCESS_INFORMATION pi;
		if (!CreateProcessA(NULL, (LPSTR)process_name, &sa, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
		{
			CloseHandle(hChildRead);
			CloseHandle(hWriteToChild);
			CloseHandle(hReadFromChild);
			CloseHandle(hChildWrite);
			return false;
		}

		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);

		return true;
	}

	// Функция отправки сообщения дочернему процессу
	__declspec(dllexport) bool __stdcall WriteToChild(const char* message, header& h)
	{
		DWORD dwWrite;

		if (h.message_size > 0)
		{
			++h.message_size; // учитываем нулевой байт в конце строки

			if (!WriteFile(hWriteToChild, &h, sizeof(header), &dwWrite, nullptr) || dwWrite == 0) // пишем заголовок
				return false;

			if (!WriteFile(hWriteToChild, message, strlen(message) + 1, &dwWrite, nullptr) || dwWrite == 0) // пишем тело сообщения
				return false;

			return true;
		}
		else
			return false;
	}

	__declspec(dllexport) int __stdcall ReadFromChild()
	{
		HANDLE input_handle = GetStdHandle(STD_ERROR_HANDLE);
		if (input_handle == INVALID_HANDLE_VALUE)
			return -1;

		DWORD dwRead;
		int message = -1;

		// Чтение из анонимного канала
		if (!ReadFile(input_handle, &message, sizeof(int), &dwRead, nullptr) || dwRead == 0)
			return message;

		return message;
	}

}

// Функция чтения заголовка сообщения
__declspec(dllexport) header __stdcall ReadHeader()
{
	header received_header{ -1,-1,-1 };

	HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
	if (input_handle == INVALID_HANDLE_VALUE)
		return received_header;

	DWORD dwRead;

	// читаем заголовок
	if (!ReadFile(input_handle, &received_header, sizeof(header), &dwRead, nullptr) || dwRead == 0)
		return header{ -1,-1,-1 };

	return received_header;
}

// Функция чтения сообщения от родительского процесса
__declspec(dllexport) string __stdcall ReadFromParent(const header& h)
{
	if (h.message_size == 0)
		return "";

	HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE); // получаем дескриптор ввода, который отвечает за прием сообщения
	if (input_handle == INVALID_HANDLE_VALUE)
		return "";
	
	char* message_buffer = new char[h.message_size];
	
	DWORD dwRead;

	// Чтение из анонимного канала
	if (!ReadFile(input_handle, message_buffer, h.message_size, &dwRead, nullptr) || dwRead == 0)
		return "";
	
	string str_message(message_buffer);

	delete[] message_buffer;

	return str_message;
}

// Функция отправки сообщения родительскому процессу
__declspec(dllexport) bool __stdcall WriteToParent(int message_code)
{
	// Получаем дескриптор ошибки, который отвечает за отправку сообщений в родительский процесс
	/*HANDLE output_handle = GetStdHandle(STD_ERROR_HANDLE);  
	if (output_handle == INVALID_HANDLE_VALUE) 
		return false;*/

	DWORD dwWrite;

	if (!WriteFile(hChildWrite, &message_code, sizeof(int), &dwWrite, nullptr) || dwWrite == 0) 
		return false;

	return true;
}

// Освобождение ресурсов анонимного канала
__declspec(dllexport) void __stdcall ClosePipeResources()
{
	CloseHandle(hWriteToChild);
	CloseHandle(hChildRead);
	CloseHandle(hChildWrite);
	CloseHandle(hReadFromChild);
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