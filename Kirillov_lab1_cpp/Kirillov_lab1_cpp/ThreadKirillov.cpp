#include "pch.h"
#include "ThreadKirillov.h"

extern std::mutex console_mtx;

ThreadKirillov::ThreadKirillov(): _id(0), _thread(nullptr), _control_event(nullptr), _receive_msg_event(nullptr), _param()
{
}

ThreadKirillov::~ThreadKirillov() // В деструкторе освобождаем ресурсы потока
{
	if(_thread != nullptr)        
		CloseHandle(_thread);
	if(_control_event != nullptr)
		CloseHandle(_control_event);

	console_mtx.lock();
	std::cout << "ID " << std::to_string(_id) << " DESTRUCTOR" << std::endl;
	console_mtx.unlock();
}

void ThreadKirillov::SetID(int id)
{
	_id = id;
}

int ThreadKirillov::GetID() const
{
	return _id;
}

// Функция создания потока, возвращает false при некорректной работе
bool ThreadKirillov::Create(AFX_THREADPROC thread_function, ParamsToThread param)
{
	if (param.id == 0 || param.control_event == NULL || param.receive_msg_event == NULL)
		return false;
	_id = param.id;
	_control_event = param.control_event;
	_receive_msg_event = param.receive_msg_event;
	_param = param;
	
	CWinThread* new_thread = AfxBeginThread(thread_function, &_param);
	if (new_thread == NULL)
		return false;

	_thread = new_thread->m_hThread;

	return true;
}

// Посылает сигнал для закрытия потока
void ThreadKirillov::Finish() 
{
	if(_control_event != nullptr)
		SetEvent(_control_event);
}

// Посылает сигнал для начала выполнения работы
void ThreadKirillov::Activate()
{
	if (_receive_msg_event != nullptr)
		SetEvent(_receive_msg_event);
}
