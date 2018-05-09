#ifndef DERRORLOGGER__H
#define DERRORLOGGER__H

#include <vector>
using namespace std;
#include <stdio.h>

class ErrorLogger
{
public:
	~ErrorLogger()
	{
		FILE * fd = fopen("errors.log", "w+");
		vector<string>::iterator i = logs.begin();
		for ( ; i != logs.end(); ++i)
			fwrite((*i).c_str(), sizeof(char), (*i).size(), fd);
		fclose(fd);
		logs.clear();
	}
	void AddString(const char * const text)
	{
		logs.push_back(string(text));
	}
	vector<string> logs;
};

#endif