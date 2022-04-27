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

enum class ThreadType  
{
    main, working
};

// Функции из dll
extern "C"
{
    __declspec(dllimport) bool __stdcall SendMappingMessage(void* message, header& h);
}
__declspec(dllimport) std::string __stdcall ReadFromParent(const header& h);
__declspec(dllimport) header __stdcall ReadHeader();

// Единственный объект приложения

CWinApp theApp;

using namespace std;

shared_mutex data_mtx;       // будет синхронизировать доступ к анонимному каналу
mutex console_mtx;    // будет синхронизировать работу консоли
HANDLE confirm_finish_of_thread_event = CreateEventA(NULL, FALSE, FALSE, NULL);  // будет сообщать о завершении потока

shared_ptr<string> ptr_received_message;

// Функция обработки сообщения в главном потоке
void ProcessMessage(shared_ptr<string>& message)
{
    shared_lock<shared_mutex> data_for_reading_lock(data_mtx); // общая блокировка, потоки могут читать данные

    lock_guard<mutex> lock_console(console_mtx);
    if (message->empty())
        cout << "MAIN THREAD FAIL: Message wasn't received or empty!" << endl;
    else
    {
        cout << "Main Thread RECEIVED Message" << endl <<
            "Size: " << message->size() << endl <<
            "Message: " << *message << endl;
    }
}

// Функция выполнения в потоке
void ThreadFunction(int thread_id, HANDLE finish_event, HANDLE receive_msg_event, weak_ptr<string>&& ptr_to_message)
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
            shared_lock<shared_mutex> data_for_reading_lock(data_mtx); // общая блокировка, данные можно только читать из потоков

            auto received_message = ptr_to_message.lock(); // пытаемся получить доступ к сообщению из weak_ptr

            if(received_message == nullptr)  // если указатель оказался висячим и данные удалены
            {
                lock_guard<mutex> lock_console(console_mtx);
                cout << "ERROR: Thread №" + to_string(thread_id) + " can't read message, because the message was deleted" << endl;
            }
            else  
            {
                if(received_message->empty())  
                {
                    lock_guard<mutex> lock_console(console_mtx);
                    cout << "ERROR: Thread №" + to_string(thread_id) + " can't read message is empty" << endl;
                    
                }
                else
                {
                    unique_lock<mutex> console_lock(console_mtx);
                    cout << "Thread №" + to_string(thread_id) + " RECEIVED Message" << endl;
                    console_lock.unlock();

                    // вывод в файл
                    ofstream outfile;
                    outfile.open("C:/repository/SysProg/L3_SysProg/OutputData/" + to_string(thread_id) + ".txt");
                    if (outfile.is_open())
                    {
                        outfile << "Message size: " << received_message->size() << endl;
                        outfile << "Message:" << endl << *received_message;
                        outfile.close();
                    }
                }
            }
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

            ptr_received_message = make_shared<string>(); // память под будущие сообщения

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

                    std::weak_ptr<string> wptr_to_message(ptr_received_message);

                    // инициализируем объект реальным потоком
                    new_thread->Init(std::thread(ThreadFunction, thread_id, thread_finish_event, thread_msg_event, std::move(wptr_to_message)));
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
                    unique_lock<shared_mutex> writing_data_lock(data_mtx); // монопольный захват мьютекса для записи нового сообщения 
                                                                           // в этот момент потоки с общей блокировкой не смогут читать

                    header msg_header = ReadHeader();    // читаем заголовок, чтобы узнать, какой поток должен читать сообщение
                    if (msg_header.message_size != 0)
                    {

                        *ptr_received_message = ReadFromParent(msg_header);  // читаем сообщение из анонимного канала
                        writing_data_lock.unlock();                          // освобождаем монопольный захват

                        switch (msg_header.thread_id)
                        {
                        case -1:                               // Чтение из всех потоков
                        {
                            ProcessMessage(ptr_received_message);
                            threads_storage.ActionAll();
                        }
                        break;

                        case 0:                                // Чтение из главного потока
                        {
                            ProcessMessage(ptr_received_message);
                        }
                        break;

                        default:                              // Чтение из произвольного потока
                        {
                            try
                            {
                                threads_storage.ActionThreadByID(msg_header.thread_id);
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