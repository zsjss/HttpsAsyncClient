#pragma once
#include "curl\curl.h"
#include "api_const.h"
#include "module\api_result.h"
#include "file_upload.h"

typedef size_t(*API_DATA_CALLBACK)(void* pBuffer, size_t nSize, size_t nMemByte, void* pParam);
typedef size_t(*API_HEADER_CALLBACK)(void* pBuffer, size_t nSize, size_t nMemByte, void* pParam);
typedef std::function<void(ApiResult)> ASYNSC_API_CALLBACK;

typedef std::map< std::string, std::string> SS_MAP;
#define WM_API_CALLBACK (WM_USER + 1001)

using namespace nim_http;
class ApiResult;
class ApiCommTool;
/*
*访问API接口
* by Darren
*/
class ApiCaller
{

public:
	ApiCaller();
	~ApiCaller();

	static void GlobalInit();
	static void GlobalCleanup();

	static bool b_global_init;

	std::string  CallGet(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_, bool need_url_encode = false);

	std::string  CallPost(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_, bool need_url_encode = false);

	std::string  CallPut(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_);

	std::string	 CallPostMultipartForm(std::string& path_, std::string &body_s_, std::list<std::string>& header_list_, ApiResult& api_result_);

	std::string  CallDelete(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_);
	static std::string CalMd5ValidSign(std::map< std::string, std::string> &param_map_);
	int	AsyncCallGet(std::string &path_, std::map< std::string, std::string> &param_map_, const ASYNSC_API_CALLBACK& response_cb, bool need_url_encode = false);
	int	AsyncCallPost(std::string &path_, std::map< std::string, std::string> &param_map_, const ASYNSC_API_CALLBACK& response_cb, bool need_url_encode = false);
private:

	//callback
	static size_t DefaultApiDataCallback(void* pBuffer, size_t nSize, size_t nMemByte, void* pParam);
	static size_t DefaultApiHeaderCallback(void* pBuffer, size_t nSize, size_t nMemByte, void* pParam);
	friend class ApiCommTool;

private:

	std::string build_user_agent_();
	std::string builde_cookie_();
	std::string assemble_general_url_(std::string path_, std::map< std::string, std::string> &param_map_);
	std::string cal_general_sign_(std::string path_, std::map< std::string, std::string> &param_map_);


	std::string assemble_post_url_(std::string path_, std::map< std::string, std::string> &param_map_);
	std::string generate_post_field_(std::map< std::string, std::string> &param_map_);


	std::string host_;
	uint16_t	port_;

	CURL*		curl;
	curl_slist* sl_headers_;

	//void*		p_response_data_;	//服务器数据
	string		s_response_;
	string		s_path_;
	//HWND m_nofify_hwnd_;

	friend bool AsyncUploadImage(ImageViewManager::ImageViewInfo& img_info_, IMAGE_TYPE it_
		, const file_upload_complete_cb& comp_cb, const file_upload_progress_cb& prog_cb, string device_id);


	friend bool AsyncUploadNimLog(const time_t gmt_start, int64_t m_biz_id, bool b_chatroom, bool b_agora);
	friend bool AsyncUploadAllLog(std::wstring account, std::string s_type);

};


