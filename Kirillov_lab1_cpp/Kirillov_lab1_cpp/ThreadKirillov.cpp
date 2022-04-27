#include "pch.h"
#include "ThreadKirillov.h"

extern std::mutex console_mtx;

ThreadKirillov::ThreadKirillov(): _id(0), _thread(), _finish_event(nullptr), _receive_msg_event(nullptr)
{
}

ThreadKirillov::~ThreadKirillov() // ¬ деструкторе освобождаем ресурсы потока
{
	if(_thread.joinable())
		_thread.detach();

	if(_finish_event != nullptr)
		CloseHandle(_finish_event);
	if (_receive_msg_event != nullptr)
		CloseHandle(_receive_msg_event);

	std::lock_guard<std::mutex> console_lock(console_mtx);
	std::cout << "ID " << std::to_string(_id) << " DESTRUCTOR" << std::endl;
}

void ThreadKirillov::SetID(int id)
{
	_id = id;
}

int ThreadKirillov::GetID() const
{
	return _id;
}

void ThreadKirillov::SetFinishEvent(HANDLE fin_event)
{
	if (fin_event != NULL)
		_finish_event = fin_event;
	else
		throw std::invalid_argument("Error! Handle is NULL!");
}

void ThreadKirillov::SetMessageEvent(HANDLE msg_event)
{
	if (msg_event != NULL)
		_receive_msg_event = msg_event;
	else
		throw std::invalid_argument("Error! Handle is NULL!");
}

void ThreadKirillov::Init(std::thread&& some_thread) noexcept
{
	_thread = std::move(some_thread);
}

// ѕосылает сигнал дл€ закрыти€ потока
void ThreadKirillov::Finish() 
{
	if(_finish_event != nullptr)
		SetEvent(_finish_event);
}

// ѕосылает сигнал дл€ начала выполнени€ работы
void ThreadKirillov::Activate()
{
	if (_receive_msg_event != nullptr)
		SetEvent(_receive_msg_event);
}
