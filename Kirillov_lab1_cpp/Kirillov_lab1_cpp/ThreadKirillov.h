#pragma once
#include "framework.h"

// �����, �������������� ������ ������ ������ ��� ������ � ���, ������ �������������, ����� ������ � ����� ������������ �������,
// ������� ��������� ���������� ������
class ThreadKirillov
{
private:
	int _id;
	HANDLE _thread;
	HANDLE _control_event;
	HANDLE _receive_msg_event;
	ParamsToThread _param;

	ThreadKirillov(ThreadKirillov&);            // ��������� ����������� � ������������
	ThreadKirillov(const ThreadKirillov&);      // ������ ������������ ����������
	ThreadKirillov& operator=(ThreadKirillov&);
public:
	ThreadKirillov();
	~ThreadKirillov();

	void SetID(int id);
	int GetID() const;
	
	bool Create(AFX_THREADPROC thread_function, ParamsToThread param);
	void Finish();

	void Activate();
};

