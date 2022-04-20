#pragma once
#include "framework.h"

//  ласс, представл€ющий обЄртку вокруг потока дл€ работы с ним, хранит идентификатор, хэндл потока и хэндл управл€ющего событи€,
// которое управл€ет поведением потока
class ThreadKirillov
{
private:
	int _id;
	std::thread _thread;
	HANDLE _finish_event;
	HANDLE _receive_msg_event;

	ThreadKirillov(ThreadKirillov&);            // запрещаем копирование и присваивание
	ThreadKirillov(const ThreadKirillov&);      // сделав конструкторы приватными
	ThreadKirillov& operator=(ThreadKirillov&);
public:
	ThreadKirillov();
	~ThreadKirillov();

	void SetID(int id);
	int GetID() const;
	void SetFinishEvent(HANDLE fin_event);
	void SetMessageEvent(HANDLE msg_event);
	
	void Init(std::thread&& some_thread) noexcept;
	void Finish();

	void Activate();
};

