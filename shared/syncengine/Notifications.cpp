/*
 *  Notifications.cpp
 *  rhosynclib
 *
 *  Created by Vlad on 2/9/09.
 *  Copyright 2009 __MyCompanyName__. All rights reserved.
 *
 */
#if defined(_WIN32_WCE)
// Fixing compiler error C2039: 'wcsftime' : is not a member of '`global namespace''
size_t __cdecl wcsftime(wchar_t *, size_t, const wchar_t *, const struct tm*);
// strdup is implemented as part of ruby CE port
extern "C" char *strdup(const char * str);
#endif

#include <string>
#include <map>

#include "Notifications.h"

static std::map<int,char*> _notifications;

extern "C" void perform_webview_refresh();
extern "C" char* get_current_location();
extern "C" char* HTTPResolveUrl(char* url);

#if defined(_WIN32_WCE)
static char* get_url(int source_id) {
	std::map<int,char*>::iterator it = _notifications.find(source_id);
	if (it!=_notifications.end()) {
		return it->second;
	}
	return NULL;
}
#else
#define get_url(source_id) _notifications.find(source_id)->second
#endif

void fire_notification(int source_id) {
	try {
		char* url = get_url(source_id);
		if (url != NULL) {
			//execute notification
			char* current_url = get_current_location();
			if (current_url && (strcmp(current_url,url)==0)) {
				printf("Executing notification (%s) for source %d\n", url, source_id);
				perform_webview_refresh();
			}
			//erase callback
			//free(url);
			//_notifications.erase(source_id);
		}
	} catch(...) {
	}
}

void free_notifications() {
	try {
		std::map<int,char*>::iterator it;
		for ( it=_notifications.begin() ; it != _notifications.end(); it++ ) {
			printf("Freeing notification (%s) for source %d\n", (char*)((*it).second), it->first);
			free((char*)((*it).second));
		}
		_notifications.clear();
	} catch(...) {
	}
}

void set_notification(int source_id, const char *url) {
	try {
		if (url) {
			char* tmp_url = get_url(source_id);
			if (tmp_url) free(tmp_url);
			tmp_url = HTTPResolveUrl(strdup(url));
			printf("Resolved [%s] in [%s]\n", url, tmp_url);								 
			printf("Setting notification (%s) for source %d\n", tmp_url, source_id);
			_notifications[source_id] = tmp_url;
		}
	} catch(...) {
	}
}

void clear_notification(int source_id) {
	try {
		char* tmp_url = get_url(source_id);
		if (tmp_url) {
			free(tmp_url);
			_notifications.erase(source_id);
		}
	} catch(...) {
	}
}