#ifndef __UTILITY_H__
#define __UTILITY_H__

template<typename T>
void DeleteBuffer(T* buf)
{
	if(buf) {
		delete [] buf;
	}
}

#endif