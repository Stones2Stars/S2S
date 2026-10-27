#pragma once

// random.h

#ifndef CIV4_RANDOM_H
#define CIV4_RANDOM_H

class CvRandom
{

public:

	DllExport CvRandom();
	DllExport virtual ~CvRandom();

	DllExport void init(unsigned long ulSeed);
	void uninit();
	void reset(unsigned long ulSeed = 0);

	DllExport uint16_t get(uint16_t usNum, const char* pszLog = NULL);  //  Returns value from 0 to num-1 inclusive.
	///<summary>
	/// get(), attributed to pCaller -- the site the synced-scope check (CvSyncedScope) reports. Wrappers pass
	/// their own caller so the report names the real call site rather than the wrapper.
	///</summary>
	uint16_t getFrom(uint16_t usNum, const char* pszLog, const void* pCaller);
	DllExport float getFloat();
	unsigned int getInt();

	void reseed(unsigned long ulNewValue);
	unsigned long getSeed();

	// for serialization
	void read(FDataStreamBase* pStream);
	void write(FDataStreamBase* pStream);

protected:

	unsigned long m_ulRandomSeed;
};

#endif
