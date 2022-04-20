#include "pch.h"
#include "ThreadStorage.h"

ThreadStorage::ThreadStorage(): _threads(0)
{
}

int ThreadStorage::GetCount() const
{
	return _threads.size();
}

// ƒобавление потока
void ThreadStorage::AddThread(std::unique_ptr<ThreadKirillov>&& somethread) 
{
	_threads.emplace_back(std::move(somethread));
}

// ѕосылает сигнал активации последнему потоку
void ThreadStorage::FinishLastThread()           
{
	if(_threads.size() > 0)
		_threads[_threads.size() - 1]->Finish();
}


void ThreadStorage::DeleteLastThread()            
{
	_threads.pop_back();
}

// очищает все потоки, у них отрабатывает деструктор, освобожда€ ресурсы
void ThreadStorage::KillAndReleaseAll() 
{
	_threads.clear();
}

//јктивирует все потоки
void ThreadStorage::ActionAll()
{
	for (auto& thrd : _threads)
		thrd->Activate();
}

// јктивирует конкретный поток
void ThreadStorage::ActionThreadByID(int id)
{
	auto thrd_iter = find_if(_threads.begin(), _threads.end(), [&](auto& some_thread) {return some_thread->GetID() == id; });

	if (thrd_iter != _threads.end())
		thrd_iter->get()->Activate();
	else
	{
		std::string error_message("Error! Thread with ID = " + std::to_string(id) + " doesn't exist!");
		throw std::exception(error_message.c_str());
	}
}



