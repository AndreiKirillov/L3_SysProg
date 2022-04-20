// FileMapping.h: основной файл заголовка для библиотеки DLL FileMapping
//

#pragma once

#ifndef __AFXWIN_H__
	#error "включить pch.h до включения этого файла в PCH"
#endif

#include "resource.h"		// основные символы


// CFileMappingApp
// Реализацию этого класса см. в файле FileMapping.cpp
//

class CFileMappingApp : public CWinApp
{
public:
	CFileMappingApp();

// Переопределение
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
