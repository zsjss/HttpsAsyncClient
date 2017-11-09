#include "HttpsClient.h"
#include <openssl/ssl.h>
#include <stdio.h>
#include "shared/log.h"
#include "utils/comm_util.h"
#include "comm_def.h"

#ifdef _DEBUG
#pragma comment(lib, "my_libcurld.lib")

#else
#pragma comment(lib, "my_libcurl.lib")

#endif

#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment ( lib, "ws2_32.lib" )
#pragma comment ( lib, "wldap32.lib" )


LONG64 httpRequestUUID = 0;
CHttpClient::CHttpClient(void) : m_bDebug(false), m_atomic(0), m_pWorkThread(NULL), m_Quit(false)
{
	m_pWorkThread = new std::thread(std::bind(&CHttpClient::workThreadFunc, this));
}

CHttpClient::~CHttpClient(void)
{
	m_Quit = true;
	if (m_pWorkThread != NULL) {
		m_pWorkThread->join();
	}
	SAFE_DELETE(m_pWorkThread);
}

static int OnDebug(CURL *, curl_infotype itype, char * pData, size_t size, void *)
{
	if (itype == CURLINFO_TEXT) {
		//printf("[TEXT]%s\n", pData);
	}
	else if (itype == CURLINFO_HEADER_IN) {
		printf("[HEADER_IN]%s\n", pData);
	}
	else if (itype == CURLINFO_HEADER_OUT) {
		printf("[HEADER_OUT]%s\n", pData);
	}
	else if (itype == CURLINFO_DATA_IN) {
		printf("[DATA_IN]%s\n", pData);
	}
	else if (itype == CURLINFO_DATA_OUT) {
		printf("[DATA_OUT]%s\n", pData);
	}
	return 0;
}

size_t CHttpClient::OnWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid)
{
	std::string* str = static_cast<std::string*> (lpVoid);
	char* pData = (char*)buffer;
	if (NULL == str || NULL == pData) {
		return 0;
	}

	str->append(pData, size * nmemb);
	return nmemb * size;
}

size_t CHttpClient::OnAsyncWriteData(void* buffer, size_t size, size_t nmemb, void* lpVoid)
{
	// CHttpRequestCollect::iterator* pit = static_cast<CHttpRequestCollect::iterator*> (lpVoid);
	string* pStr = static_cast<std::string*>(lpVoid);
	char* pData = (char*)buffer;
	if (NULL == pStr || NULL == pData) {
		return 0;
	}

	pStr->append(pData, size * nmemb);
	return nmemb * size;
}

int CHttpClient::Get(const std::string & strUrl, std::string & strResponse)
{
	CURLcode res;
	CURL* curl = curl_easy_init();
	if (NULL == curl) {
		return CURLE_FAILED_INIT;
	}
	if (m_bDebug) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, OnDebug);
	}
	curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);
	/**
	* ������̶߳�ʹ�ó�ʱ�����ʱ��ͬʱ���߳�����sleep����wait�Ȳ�����
	* ������������ѡ�libcurl���ᷢ�źŴ�����wait�Ӷ����³����˳���
	*/
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, m_connectTimeOut);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_requestTimeOut);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
	curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //����һ��֤���ļ�  

	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return res;
}

int CHttpClient::asyncGet(const std::string & strUrl, curl_slist* sl_headers, HttpCallBack cb)
{
	CHttpRequestPtr ptr(new CHttpRequest(true, sl_headers,strUrl, string(""), cb));

	nbase::NAutoLock auto_lock(&m_NewRequestMutex);
	
	m_NewRequestCollect.push_back(ptr);

	return 0;
}

int CHttpClient::Post(const std::string & strUrl, const std::string & strPost, std::string & strResponse)
{
	CURLcode res;
	CURL* curl = curl_easy_init();
	if (NULL == curl) {
		return CURLE_FAILED_INIT;
	}
	if (m_bDebug) {
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, OnDebug);
	}
	curl_easy_setopt(curl, CURLOPT_URL, strUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost.c_str());
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&strResponse);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, m_connectTimeOut);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, m_requestTimeOut);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
	curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //����һ��֤���ļ�

	res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	return res;
}

LONG64 CHttpClient::asyncPost(const std::string & strUrl, const std::string & strPost, curl_slist* sl_headers, HttpCallBack cb)
{
	CHttpRequestPtr ptr(new CHttpRequest(false,sl_headers,strUrl, strPost, cb));

	nbase::NAutoLock auto_lock(&m_NewRequestMutex);
	m_NewRequestCollect.push_back(ptr);
	//ptr->m_cbObj = obj;
	return ptr->uuid;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void CHttpClient::SetDebug(bool bDebug)
{
	m_bDebug = bDebug;
}

void CHttpClient::workThreadFunc()
{
	curl_global_init(CURL_GLOBAL_ALL);
	m_handMultiCurl = curl_multi_init();
	bool debug = false;
	while (m_Quit == false) {
		Sleep(50);
		//ȡ��request
		// ����easy curl������ӵ�multi curl������  //
		CHttpRequestCollect tmp;
		{
			nbase::NAutoLock auto_lock(&m_NewRequestMutex);
			tmp = m_NewRequestCollect;
			m_NewRequestCollect.clear();
		}


		CHttpRequestCollect::iterator it = tmp.begin();
		CHttpRequestCollect::iterator itend = tmp.end();
		for (; it != itend; ++it) {
			CURL* hUrl = curl_easy_handler((*it)->b_get_,(*it)->m_url, (*it)->m_postData, it);
			if (hUrl == NULL) {
				continue;
			}
			m_DoingRequestCollect.insert(std::make_pair((LONG64)hUrl, *it));

			curl_multi_add_handle(m_handMultiCurl, hUrl);

		}
		if (debug)
			cout << "add handler finish" << endl;
		/*
		* ����curl_multi_perform����ִ��curl����
		* url_multi_perform����CURLM_CALL_MULTI_PERFORMʱ����ʾ��Ҫ�������øú���ֱ������ֵ����CURLM_CALL_MULTI_PERFORMΪֹ
		* running_handles�����������ڴ����easy curl������running_handlesΪ0��ʾ��ǰû������ִ�е�curl����
		*/
		int running_handles;
		while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(m_handMultiCurl, &running_handles)) {
			//cout << running_handles << endl;
		}
		if (debug)
			cout << "exec request finish" << endl;
		/**
		* Ϊ�˱���ѭ������curl_multi_perform������cpu����ռ�õ����⣬����select�������ļ�������
		*/
		//        while (running_handles)
		//        {
		if (running_handles > 0) {
			if (-1 == curl_multi_select(m_handMultiCurl)) {
				//cerr << "select error" << endl;
				continue;
			}
		}
		// select�������¼�������curl_multi_perform֪ͨcurlִ����Ӧ�Ĳ��� //
		//        while (CURLM_CALL_MULTI_PERFORM == curl_multi_perform(m_handMultiCurl, &running_handles));
		//        {
		//            cout << "select2: " << running_handles << endl;
		//        }
		//cout << "select: " << running_handles << endl;
		//        }

		// ���ִ�н�� //
		int msgs_left = 0;
		CURLMsg * msg=NULL;
		while (1) {
			msg = curl_multi_info_read(m_handMultiCurl, &msgs_left);
			if (!msg)
			{
				break;
			}
			if (CURLMSG_DONE == msg->msg) {
				//CHttpRequestCollect::iterator itd;
				//curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &itd);
				CHttpRequestMap::iterator itd = m_DoingRequestCollect.find((LONG64)msg->easy_handle);
				if (itd != m_DoingRequestCollect.end()) {

					(itd->second)->m_state = msg->data.result;
					{
						nbase::NAutoLock auto_lock(&m_FinishRequestMutex);
						m_FinishRequestCollect.push_back(itd->second);
					}

					CHttpRequestPtr chttp = itd->second;
					
					if (chttp)
					{
						chttp->ClearSlHeader();
						if (chttp->m_callBack)
						{
							chttp->m_callBack(chttp);
						}
					}

					m_DoingRequestCollect.erase(itd);
					curl_multi_remove_handle(m_handMultiCurl, msg->easy_handle);
					curl_easy_cleanup(msg->easy_handle);
				}

			}
			else {
				cerr << "bad request, result:" << msg->msg << endl;
			}

		}
		if (debug)
			cout << "read info finish:" << endl;

	}
	curl_multi_cleanup(m_handMultiCurl);
	curl_global_cleanup();
}

int CHttpClient::curl_multi_select(CURLM * curl_m)
{
	int ret = 0;

	struct timeval timeout_tv;
	fd_set fd_read;
	fd_set fd_write;
	fd_set fd_except;
	int max_fd = -1;

	// ע������һ��Ҫ���fdset,curl_multi_fdset����ִ��fdset����ղ���  //
	FD_ZERO(&fd_read);
	FD_ZERO(&fd_write);
	FD_ZERO(&fd_except);

	// ����select��ʱʱ��  //
	timeout_tv.tv_sec = 0;
	timeout_tv.tv_usec = 50000;

	// ��ȡmulti curl��Ҫ�������ļ����������� fd_set //
	CURLMcode mc = curl_multi_fdset(curl_m, &fd_read, &fd_write, &fd_except, &max_fd);

	if (mc != CURLM_OK) {
		fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
		return -1;
	}

	/**
	* When max_fd returns with -1,
	* you need to wait a while and then proceed and call curl_multi_perform anyway.
	* How long to wait? I would suggest 100 milliseconds at least,
	* but you may want to test it out in your own particular conditions to find a suitable value.
	*/
	if (-1 == max_fd) {
		return -1;
	}

	/**
	* ִ�м��������ļ�������״̬�����ı��ʱ�򷵻�
	* ����0���������curl_multi_perform֪ͨcurlִ����Ӧ����
	* ����-1����ʾselect����
	* ע�⣺��ʹselect��ʱҲ��Ҫ����0���������ȥ�������ĵ�˵��
	*/
	int ret_code = ::select(max_fd + 1, &fd_read, &fd_write, &fd_except, &timeout_tv);
	switch (ret_code) {
	case -1:
		/* select error */
		ret = -1;
		break;
	case 0:
		/* select timeout */
	default:
		/* one or more of curl's file descriptors say there's data to read or write*/
		ret = 0;
		break;
	}

	return ret;
}

CURL* CHttpClient::curl_easy_handler(bool b_get,std::string & sUrl, std::string & strPost, CHttpRequestCollect::iterator it, uint32 uiTimeout)
{
	std::string ua_("");

	ua_.append("os/").append(GetOSInfoEx());
	ua_.append(" da_version/").append(APP_INNER_VERSION);
	ua_.append(" os_version/").append(GetOSVersion());
	ua_.append(" jiayouxueba/").append(APP_VERSION);
	ua_.append(" device/PCMAC_").append(GetMac());
	ua_.append(" api/").append(JYXB_API_VERSION);

	CURL * curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, sUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, m_requestTimeOut*1000.0f);

	if (!b_get)
	{
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strPost.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strPost.length());
	}
	
	// write function //
	CHttpRequestPtr request_ptr = (*it);
	if (request_ptr)
	{
		curl_slist* sl_headers = request_ptr->sl_headers_;
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sl_headers);// ��Э��ͷ
	}
	
	curl_easy_setopt(curl, CURLOPT_USERAGENT, ua_.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &((*it)->m_response));
	curl_easy_setopt(curl, CURLOPT_PRIVATE, it);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, m_connectTimeOut*1000.0f);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
	curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //����һ��֤���ļ�  

	return curl;
}

void CHttpClient::update()
{
	CHttpRequestCollect temp;
	{
		nbase::NAutoLock auto_lock(&m_FinishRequestMutex);
		temp = m_FinishRequestCollect;
		m_FinishRequestCollect.clear();
	}


	uint32 s = m_unregisterRequestCollect.size();
	CHttpRequestCollect::iterator it = temp.begin();
	CHttpRequestCollect::iterator itend = temp.end();
	for (; it != itend; ++it)
	{
		CHttpRequestPtr& req = (*it);
		if (s > 0)
		{
			auto itSet = m_unregisterRequestCollect.find(req->uuid);
			if (itSet != m_unregisterRequestCollect.end())
			{
				m_unregisterRequestCollect.erase(itSet);
				continue;
			}
		}
		
		if (req->m_state != CURLE_OK)
		{
			QLOG_ERR(L"*Error:HttpRequest error Code={0}  UUID={1}  URL={2}  postData={3}   Response={4}") << req->m_state << req->uuid << req->m_url << req->m_postData << req->m_response;
		}
		else
		{
			QLOG_ERR(L"HttpRequest CallBack, UUID={0}  URL={1}  Response={2}") << req->uuid << req->m_url << req->m_postData << req->m_response;
		}

		((*it)->m_callBack)(req);
	}
}

bool CHttpClient::unregisterCallBack(LONG64 requestUUID)
{
	m_unregisterRequestCollect.insert(requestUUID);
	return true;
}

//------------------------------------------------------------------------------------

CHttpRequest::CHttpRequest(bool b_get, curl_slist* sl_headers,const std::string& url, const std::string& postData, HttpCallBack cb) : b_get_(b_get), sl_headers_(sl_headers), m_url(url), m_postData(postData), m_callBack(cb), m_state(CURL_LAST)
{
	uuid = httpRequestUUID++;
}