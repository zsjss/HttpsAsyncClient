#pragma once

#ifndef __HTTP_CURL_H__
#define __HTTP_CURL_H__

#include "base/memory/singleton.h"
#include "libyuv/include/libyuv/basic_types.h"
#include <thread>
#include "curl/curl.h"

class CHttpRequest;

typedef std::shared_ptr<CHttpRequest> CHttpRequestPtr;
typedef std::function<void(const CHttpRequestPtr &) > HttpCallBack;
typedef std::map<LONG64, CHttpRequestPtr> CHttpRequestMap;
typedef std::list<CHttpRequestPtr> CHttpRequestCollect;
typedef std::set<LONG64> CHttpRequestIDSet;

using namespace std;
class CHttpClient
{
public:
	SINGLETON_DEFINE(CHttpClient);
	CHttpClient(void);
	virtual ~CHttpClient(void);

public:
	//ˢ֡����
	void update();

	/**
	* @brief HTTP �첽Post����
	* @param strUrl �����Url��ַ
	* @param strPost Post����
	* @param cb �ص�����
	* @return
	*/
	LONG64 asyncPost(const std::string & strUrl, const std::string & strPost, curl_slist* sl_headers, HttpCallBack cb);

	int asyncGet(const std::string & strUrl, curl_slist* sl_headers, HttpCallBack cb);

	/**
	* @brief HTTP GET����
	* @param strUrl �������,�����Url��ַ,��:http://www.baidu.com
	* @param strResponse �������,���ص�����
	* @return �����Ƿ�Post�ɹ�
	*/

	/**
	* @brief ע�����ص�����;
	* @param obj
	*/
	bool unregisterCallBack(LONG64 requestUUID);


	int Get(const std::string & strUrl, std::string & strResponse);
	/**
	* @brief HTTP POST����
	* @param strUrl �������,�����Url��ַ,��:http://www.baidu.com
	* @param strPost �������,ʹ�����¸�ʽpara1=val&para2=val2&��
	* @param strResponse �������,���ص�����
	* @return �����Ƿ�Post�ɹ�
	*/
	int Post(const std::string & strUrl, const std::string & strPost, std::string & strResponse);

	void SetDebug(bool bDebug);

	void setConnectTimeOut(float t){ m_connectTimeOut = t; }
	void setRequestTimeOut(float t){ m_requestTimeOut = t; }
	

private:
	int curl_multi_select(CURLM * curl_m);
	CURL* curl_easy_handler(bool b_get, std::string & sUrl, std::string & strPost, CHttpRequestCollect::iterator it, uint32 uiTimeout = 10000);
	void workThreadFunc();

	/**
	* @brief HTTPS POST����,��֤��汾
	* @param strUrl �������,�����Url��ַ,��:https://www.alipay.com
	* @param strPost �������,ʹ�����¸�ʽpara1=val&para2=val2&��
	* @param strResponse �������,���ص�����
	* @param pCaPath �������,ΪCA֤���·��.�������ΪNULL,����֤��������֤�����Ч��.
	* @return �����Ƿ�Post�ɹ�
	*/
	int Posts(const std::string & strUrl, const std::string & strPost, std::string & strResponse, const char * pCaPath = NULL);

	/**
	* @brief HTTPS GET����,��֤��汾
	* @param strUrl �������,�����Url��ַ,��:https://www.alipay.com
	* @param strResponse �������,���ص�����
	* @param pCaPath �������,ΪCA֤���·��.�������ΪNULL,����֤��������֤�����Ч��.
	* @return �����Ƿ�Post�ɹ�
	*/
	int Gets(const std::string & strUrl, std::string & strResponse, const char * pCaPath = NULL);
	
	static size_t OnWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid);
	static size_t OnAsyncWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid);

private:
	bool m_bDebug;

	float m_connectTimeOut = 20.0f;
	float m_requestTimeOut = 30.0f;

	uint32 m_atomic;

	CHttpRequestCollect m_NewRequestCollect;


	nbase::NLock m_NewRequestMutex;

	CHttpRequestMap m_DoingRequestCollect;

	CHttpRequestCollect m_FinishRequestCollect;

	nbase::NLock m_FinishRequestMutex;

	CURLM * m_handMultiCurl;

	std::thread* m_pWorkThread;
	

	bool m_Quit;

	CHttpRequestIDSet m_unregisterRequestCollect;
};

class CHttpRequest
{
public:
	CHttpRequest(bool b_get, curl_slist* sl_headers, const std::string& url, const std::string& postData, HttpCallBack cb);

	~CHttpRequest()
	{
		
	};

	void setResponse(const std::string& resp)
	{
		m_response = resp;
	};

	void ClearSlHeader()
	{
		if (sl_headers_)
		{
			curl_slist_free_all(sl_headers_);
		}
	}

	LONG64 uuid;
	bool b_get_;
	std::string m_url;
	std::string m_postData;
	std::string m_response;
	HttpCallBack m_callBack;

	curl_slist* sl_headers_;//����http header
	CURLcode m_state; //������ ����0�ɹ�����ʧ��
};

#endif
