#include <assert.h>
#include <stdlib.h>

#include <ruby.h>

#include <common/RhoDefs.h>

RHO_GLOBAL void* openPhonebook()
{
	// TODO:
	RHO_ABORT("Not implemented");
	return NULL;
}

RHO_GLOBAL void closePhonebook(void* pb)
{
	// TODO:
	RHO_ABORT("Not implemented");
}

RHO_GLOBAL VALUE getallPhonebookRecords(void* pb)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

RHO_GLOBAL void* openPhonebookRecord(void* pb, char* id)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return NULL;
}

RHO_GLOBAL VALUE getPhonebookRecord(void* pb, char* id)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

RHO_GLOBAL VALUE getfirstPhonebookRecord(void* pb)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

RHO_GLOBAL VALUE getnextPhonebookRecord(void* pb)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

RHO_GLOBAL void* createRecord(void* pb)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return NULL;
}

RHO_GLOBAL int setRecordValue(void* record, char* property, char* value)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

RHO_GLOBAL int addRecord(void* pb, void* record)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

RHO_GLOBAL int saveRecord(void* pb, void* record)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

RHO_GLOBAL int deleteRecord(void* pb, void* record)
{
	// TODO:
	RHO_ABORT("Not implemented");
	return 0;
}

