#pragma once
#include "framework.h"
#include "ThreadKirillov.h"

// Класс, позволяющий хранить потоки и работать с ними
class ThreadStorage
{
private:
	std::vector<std::unique_ptr<ThreadKirillov>> _threads;

	ThreadStorage(ThreadStorage&);               // запрещаем копирование и присваивание
	ThreadStorage(const ThreadStorage&);		 // сделав конструкторы приватными
	ThreadStorage& operator=(ThreadStorage&);
public:
	ThreadStorage();

	int GetCount() const;

	void AddThread(std::unique_ptr<ThreadKirillov>&& somethread);

	void FinishLastThread();
	void DeleteLastThread();
	void KillAndReleaseAll();

	void ActionAll();
	void ActionThreadByID(int id);

};

