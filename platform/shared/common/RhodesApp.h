#pragma once

#ifdef __cplusplus

#include "logging/RhoLog.h"
#include "common/RhoThread.h"

struct shttpd_ctx;

namespace rho {
namespace common {

class CRhodesApp : public common::CRhoThread
{
public:
    DEFINE_LOGCLASS;
private:

    static CRhodesApp* m_pInstance;
    CRhodesApp(const String& strRootPath);
    boolean m_bExit;

    String m_strListeningPorts;
    struct shttpd_ctx * m_shttpdCtx;
    String m_strRhoRootPath, m_strLoadingPagePath, m_strBlobsDirPath;
    String m_strHomeUrl, m_strStartUrl, m_strOptionsUrl, m_strRhobundleReloadUrl, m_strCurrentUrl, m_strFirstStartUrl;

    common::CMutex m_mxViewMenuItems;
    Hashtable<String,String> m_hashViewMenuItems;
    String m_strAppBackUrl;

public:
    ~CRhodesApp(void);

    static CRhodesApp* Create(const String& strRootPath);
    static void Destroy();
    static CRhodesApp* getInstance(){ return m_pInstance; }
	void startApp();
    void stopApp();

    String canonicalizeRhoUrl(const String& strUrl) ;
    void  keepLastVisitedUrl(String strUrl);
    void navigateToUrl( const String& strUrl);

    const String& getRhobundleReloadUrl();
    const String& getStartUrl();
    const String& getOptionsUrl();
    const String& getCurrentUrl();
    const String& getFirstStartUrl();

    const String& getLoadingPagePath(){return m_strLoadingPagePath; }
    const String& getBlobsDirPath(){return m_strBlobsDirPath; }
    const String& getRhoRootPath(){return m_strRhoRootPath;}

    const String& getAppBackUrl(){return m_strAppBackUrl;}

    void callCameraCallback(String strCallbackUrl, const String& strImagePath, const String& strError, boolean bCancel );
    void callDateTimeCallback(String strCallbackUrl, long lDateTime, const char* szData, int bCancel );
    void callAppActiveCallback();

    void setViewMenu(unsigned long valMenu);
    void addViewMenuItem( const String& strLabel, const String& strLink );

private:
	virtual void run();

    void initHttpServer();
    void initAppUrls();

    const char* getFreeListeningPort();

};

}
}

inline rho::common::CRhodesApp& RHODESAPP(){ return *rho::common::CRhodesApp::getInstance(); }

#endif //__cplusplus

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
	
void rho_rhodesapp_create(const char* szRootPath);
void rho_rhodesapp_start();	
void rho_rhodesapp_destroy();
const char* rho_native_rhopath();
	
const char* rho_rhodesapp_getstarturl();
const char* rho_rhodesapp_getfirststarturl();

const char* rho_rhodesapp_getoptionsurl();
void rho_rhodesapp_keeplastvisitedurl(const char* szUrl);
const char* rho_rhodesapp_getcurrenturl();

const char* rho_rhodesapp_getloadingpagepath();
const char* rho_rhodesapp_getblobsdirpath();
	
void rho_http_redirect(void* httpContext, const char* szUrl);
void rho_http_senderror(void* httpContext, int nError, const char* szMsg);
void rho_http_sendresponse(void* httpContext, const char* szBody);

char* rho_http_normalizeurl(const char* szUrl);
void rho_http_free(void* data);

void rho_rhodesapp_callCameraCallback(const char* strCallbackUrl, const char* strImagePath, 
    const char* strError, int bCancel );
void rho_rhodesapp_callDateTimeCallback(const char* strCallbackUrl, long lDateTime, const char* szData, int bCancel );
void rho_rhodesapp_callAppActiveCallback();

void rho_rhodesapp_setViewMenu(unsigned long valMenu);
const char* rho_rhodesapp_getappbackurl();

#ifdef __cplusplus
};
#endif //__cplusplus
