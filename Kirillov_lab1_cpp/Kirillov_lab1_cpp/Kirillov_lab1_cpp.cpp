// Kirillov_lab1_cpp.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

//#define _MAIN true
//#define _WORKING false

#include "pch.h"
#include "framework.h"
#include "Kirillov_lab1_cpp.h"
#include "ThreadKirillov.h"
#include "ThreadStorage.h"
#include "FileMapping.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

struct header // заголовок для сообщения
{
    int thread_id;
    int message_size;
};

// Функции из dll
extern "C"
{
    __declspec(dllimport) bool __stdcall SendMappingMessage(void* message, header& h);
}
__declspec(dllimport) std::string __stdcall ReadMessage(header& h);
__declspec(dllimport) header __stdcall ReadHeader();

// Единственный объект приложения

CWinApp theApp;

using namespace std;

mutex data_mtx;       // будет синхронизировать доступ к отображаемой памяти
mutex console_mtx;    // будет синхронизировать работу консоли
HANDLE confirm_finish_of_thread_event = CreateEventA(NULL, FALSE, FALSE, NULL);  // будет сообщать о завершении потока

void ReceiveAndProcessMessage(bool thread_type, int thread_id = 0)
{
    header h;
    unique_lock<mutex> lock_data_mtx(data_mtx);          // Синхронизируем чтение из памяти
    string received_message = ReadMessage(h);
    lock_data_mtx.unlock();
    if (thread_type)
    {
        lock_guard<mutex> lock_console(console_mtx);
        if (received_message == "")
            cout << "MAIN THREAD FAIL: Message wasn't received or empty!" << endl;
        else
        {
            cout << "Main Thread RECEIVED Message" << endl <<
                "Size: " << h.message_size << endl <<
                "Message: " << received_message << endl;
        }
    }
    else
    {
        if (received_message == "")
        {
            lock_guard<mutex> lock_console(console_mtx);  
            cout << "Thread №" + to_string(thread_id) + "FAIL: Message wasn't received or empty!" << endl;
        }
        else
        {
            console_mtx.lock();
            cout << "Thread №" + to_string(thread_id) + " RECEIVED Message" << endl;
            console_mtx.unlock();
            ofstream outfile;
            outfile.open("C:/repository/SysProg/L3_SysProg/OutputData/" + to_string(thread_id) + ".txt");
            if (outfile.is_open())
            {
                outfile << "Message size: " << to_string(h.message_size) << endl;
                outfile << "Message:" << endl << received_message;
                outfile.close();
            }
        }
    }
}

void ThreadFunction(int thread_id, HANDLE finish_event, HANDLE receive_msg_event) // Функция выполнения в потоке
{
    console_mtx.lock();
    cout << "Thread №" + to_string(thread_id) + " START" << endl;
    console_mtx.unlock();

    HANDLE hControlEvents[2] = {receive_msg_event, finish_event};
    while (true)
    {
        int event_index = WaitForMultipleObjects(2, hControlEvents, FALSE, INFINITE);     // Ждём сигнал от события
        switch (event_index)
        {
        case 0:// событие получения сообщения
        {
            ReceiveAndProcessMessage(_WORKING, thread_id);
        }
        break;

        case 1: // событие завершения потока
        {
            lock_guard<mutex> lock_console(console_mtx);
            cout << "Thread №" + to_string(thread_id) + " IS CLOSED" << endl;
            SetEvent(confirm_finish_of_thread_event);
            return;
        }
        }
    }
}

void CloseAllObjects(list<HANDLE> handles)    // Освобождение русурсов всех объектов ядра
{
    for (auto event : handles)
    {
        CloseHandle(event);
    }
}

int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

    if (hModule != nullptr)
    {
        // инициализировать MFC, а также печать и сообщения об ошибках про сбое
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
            // TODO: вставьте сюда код для приложения.
            wprintf(L"Критическая ошибка: сбой при инициализации MFC\n");
            nRetCode = 1;
        }
        else
        {
            setlocale(LC_ALL, "Russian");

            // список программных событий
            list<HANDLE> kernel_objects; 
            HANDLE create_thread_event = CreateEventA(NULL, FALSE, FALSE, "CreateNewThread");
            kernel_objects.push_back(create_thread_event);

            HANDLE close_thread_event = CreateEventA(NULL, FALSE, FALSE, "CloseThread");
            kernel_objects.push_back(close_thread_event);

            HANDLE confirm_event = CreateEventA(NULL, FALSE, FALSE, "ConfirmEvent");
            kernel_objects.push_back(confirm_event);

            HANDLE close_programm_event = CreateEventA(NULL, FALSE, FALSE, "CloseProgrammEvent");
            kernel_objects.push_back(close_programm_event);

            HANDLE message_event = CreateEventA(NULL, FALSE, FALSE, "SendMessage");
            kernel_objects.push_back(message_event);

            HANDLE error_event = CreateEventA(NULL, FALSE, FALSE, "ErrorEvent");
            kernel_objects.push_back(error_event);

            HANDLE hControlEvents[4] = { create_thread_event, close_thread_event, message_event, close_programm_event };

            ThreadStorage threads_storage;
            SetEvent(confirm_event);   // подтвердение запуска приложения
            
            while (true)
            {
                int event_index = WaitForMultipleObjects(4, hControlEvents, FALSE, INFINITE) - WAIT_OBJECT_0; // Ждём событие от 
                                                                                                              // главной программы
                switch (event_index)
                {
                case 0:         // Событие создания потока
                {
                    std::unique_ptr<ThreadKirillov> new_thread = std::make_unique<ThreadKirillov>(); // Новый объект потока

                    // параметры для потока
                    int thread_id = threads_storage.GetCount() + 1;
                    HANDLE thread_finish_event = CreateEventA(NULL, FALSE, FALSE, NULL);
                    HANDLE thread_msg_event = CreateEventA(NULL, FALSE, FALSE, NULL);
                    if (thread_finish_event == NULL || thread_msg_event == NULL)
                    {
                        //SetEvent(error_event);
                        break;
                    }

                    // инициализируем объект реальным потоком
                    new_thread->Init(std::thread(ThreadFunction, thread_id, thread_finish_event, thread_msg_event));
                    new_thread->SetID(thread_id);
                    new_thread->SetFinishEvent(thread_finish_event);
                    new_thread->SetMessageEvent(thread_msg_event);

                    threads_storage.AddThread(std::move(new_thread));
                    SetEvent(confirm_event);
                }
                break;

                case 1:              // Событие завершения потока
                {
                    if (threads_storage.GetCount() > 0)
                    {
                        threads_storage.FinishLastThread();
                        WaitForSingleObject(confirm_finish_of_thread_event, INFINITE); // Ждём завершение потока
                        threads_storage.DeleteLastThread();                            // Только после этого освобождаем ресурсы
                        SetEvent(confirm_event);
                    }
                    else      // Освобождаем все ресурсы, если нет активных потоков
                    {
                        SetEvent(close_programm_event);
                        threads_storage.KillAndReleaseAll();
                        CloseAllObjects(kernel_objects);
                        CloseHandle(confirm_finish_of_thread_event);
                        return 0;
                    }
                }
                break;

                case 2:
                {
                    header h = ReadHeader();    // читаем заголовок, чтобы узнать, какой поток должен читать сообщение
                    if (h.message_size != 0)    
                    {
                        switch (h.thread_id)
                        {
                        case -1:                               // Чтение из всех потоков
                        {
                            ReceiveAndProcessMessage(_MAIN);
                            threads_storage.ActionAll();
                        }
                        break;

                        case 0:                                // Чтение из главного потока
                        {
                            ReceiveAndProcessMessage(_MAIN);
                        }
                        break;

                        default:                              // Чтение из произвольного потока
                        {
                            try
                            {
                                threads_storage.ActionThreadByID(h.thread_id);
                            }
                            catch (exception ex)             // вдруг нет потока с данным id
                            {
                                console_mtx.lock();
                                cout << ex.what() << endl;
                                console_mtx.unlock();
                            }
                        }
                        }

                    }
                    SetEvent(confirm_event);
                }
                break;

                case 3:      // Закрытие программы
                {
                    SetEvent(close_programm_event);
                    threads_storage.KillAndReleaseAll();
                    CloseAllObjects(kernel_objects);
                    CloseHandle(confirm_finish_of_thread_event);
                    return 0;
                }
                }
            }

        }
    }
    else
    {
        // TODO: измените код ошибки в соответствии с потребностями
        wprintf(L"Критическая ошибка: сбой GetModuleHandle\n");
        nRetCode = 1;
    }

    return nRetCode;
}