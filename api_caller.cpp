#include "api_caller.h"
#include "encrypt\encrypt.h"
#include "utils\comm_util.h"
#include "user_center/t_user_info.h"
#include "module\api_result.h"
#include "comm_def.h"
#include "encrypt/url_util.h"
#include <thread>
#include "utils/api_comm_tool.h"
#include "global/global_setting.h"
#include "openssl/ossl_typ.h"
#include "openssl/bio.h"
#include "openssl/pem.h"
#include "openssl/ssl.h"

#ifdef _DEBUG
#pragma comment(lib, "my_libcurld.lib")

#else
#pragma comment(lib, "my_libcurl.lib")

#endif

#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment ( lib, "ws2_32.lib" )
#pragma comment ( lib, "wldap32.lib" )

//#pragma comment(lib, "user32.lib")
bool ApiCaller::b_global_init = false;

ApiCaller::ApiCaller() :host_(API_HOST), port_(API_PORT), s_response_("")
{
	curl = curl_easy_init();    // 初始化
	sl_headers_ = nullptr;

}

ApiCaller::~ApiCaller()
{
	if (sl_headers_)
	{
		curl_slist_free_all(sl_headers_);
	}
	if (curl)
	{
		curl_easy_cleanup(curl);
	}

}

void ApiCaller::GlobalInit()
{
	if (!b_global_init)
	{
		CURLcode cc = curl_global_init(CURL_GLOBAL_ALL);
		b_global_init = (cc == CURLE_OK);
	}

}

void ApiCaller::GlobalCleanup()
{
	if (b_global_init)
	{
		curl_global_cleanup();
	}
}

std::string ApiCaller::build_user_agent_()
{
	std::string ua_("");

	ua_.append("os/").append(GetOSInfoEx());
	ua_.append(" da_version/").append(APP_INNER_VERSION);
	ua_.append(" os_version/").append(GetOSVersion());
	ua_.append(" jiayouxueba/").append(APP_VERSION);
	ua_.append(" device/PCMAC_").append(GetMac());
	ua_.append(" api/").append(JYXB_API_VERSION);

	return ua_;

}

std::string ApiCaller::builde_cookie_()
{
	string cookie_("Cookie: ");

	if (UserInfo::GetInstance()->GetLoginCookie().empty())
	{
		cookie_.append(GlobalSetting::GetInstance()->GetLoginAuth());
	}
	else
	{
		cookie_.append(UserInfo::GetInstance()->GetLoginCookie());
	}

	return cookie_;
}


std::string ApiCaller::cal_general_sign_(std::string path_, std::map< std::string, std::string> &param_map_)
{
	vector<std::string> p_vector = vector<std::string>();

	//添加时间戳
	time_t ts_ = time(NULL);

	srand(ts_);

	int r = rand() % 999999;

	std::string ts_s_ = nbase::StringPrintf("%06d%d", r, ts_).append(GetMac());
	param_map_.insert(std::make_pair("ts", ts_s_));

	//添加key path
	p_vector.push_back(API_SIGN_KEY);
	p_vector.push_back(path_);

	auto iter_ = param_map_.begin();
	while (iter_ != param_map_.end())
	{
		p_vector.push_back(iter_->second);
		iter_++;
	}

	sort(p_vector.begin(), p_vector.end());

	std::string str_("");
	for (int i = 0; i < p_vector.size(); i++)
	{
		str_.append(p_vector[i]);
	}

	std::string cal_md5_;
	CalMd5(str_, cal_md5_);
	param_map_.insert(std::make_pair("sign", cal_md5_));

	return cal_md5_;
}

std::string ApiCaller::assemble_post_url_(std::string path_, std::map< std::string, std::string>& param_map_)
{
	s_path_ = path_;
	//先计算签名
	cal_general_sign_(path_, param_map_);

	std::string _url(host_);
	if (port_ != 80)
	{
		_url.append(":").append(nbase::IntToString(port_));

	}
	_url.append(API_CONTEXT).append(path_);

	_url.append("?ts=").append(param_map_["ts"]);
	_url.append("&sign=").append(param_map_["sign"]);

	return _url;

}



std::string ApiCaller::assemble_general_url_(std::string path_, std::map< std::string, std::string>& param_map_)
{
	s_path_ = path_;
	//先计算签名
	cal_general_sign_(path_, param_map_);

	std::string _url(host_);
	if (port_ != 80)
	{
		_url.append(":").append(nbase::IntToString(port_));

	}
	_url.append(API_CONTEXT).append(path_);

	if (!param_map_.empty())
	{

		std::map< std::string, std::string>::iterator iter_ = param_map_.begin();

		while (iter_ != param_map_.end())
		{
			iter_ == param_map_.begin() ? _url.append("?") : _url.append("&");
			_url.append(iter_->first).append("=").append(iter_->second);
			iter_++;
		}
	}
	return _url;

}

size_t ApiCaller::DefaultApiDataCallback(void* pBuffer, size_t nSize, size_t nMemByte, void* pParam)
{
	assert(pParam);
	ApiCaller* p_api_ = (ApiCaller*)pParam;

	QLOG_APP(L"call api {0} result {1}") << p_api_->s_path_ << string((char*)(pBuffer));
	p_api_->s_response_.append((char*)pBuffer);

	return nSize * nMemByte;
}

size_t ApiCaller::DefaultApiHeaderCallback(void* pBuffer, size_t nSize, size_t nMemByte, void* pParam)
{
	//QLOG_APP(L"default api header callback {0}") << string((char*)(pBuffer));
	return nSize * nMemByte;
}

std::string ApiCaller::CallGet(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_, bool need_url_encode)
{
	for (auto i = param_map_.begin(); need_url_encode && i != param_map_.end(); i++)
	{
		string& v = i->second;
		v = urlencode(v);
	}

	CURLcode res;
	std::string s_ret_("");

	if (curl)
	{
		std::string url = assemble_general_url_(path_, param_map_);
		std::string ua = build_user_agent_();

		QLOG_APP(L"call api {0}") << url;

		sl_headers_ = curl_slist_append(sl_headers_, "Accept: */*");
		sl_headers_ = curl_slist_append(sl_headers_, builde_cookie_().c_str());
		string ts_str = "xy-nonce: " + param_map_["ts"];
		sl_headers_ = curl_slist_append(sl_headers_, ts_str.c_str());
		string sign_str = "xy-sign: " + param_map_["sign"];
		sl_headers_ = curl_slist_append(sl_headers_, sign_str.c_str());
		sl_headers_ = curl_slist_append(sl_headers_, "xy-access-key: jyxb");

		if (strncmp(url.c_str(), "http://", 7) == 0)
		{
			// 方法1, 设定为不验证证书和HOST  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		}
		else if (strncmp(url.c_str(), "https://", 8) == 0)
		{
			// 方法2, 设定一个SSL判别证书, 未测试  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
			curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //设置一个证书文件  
		}

		//curl_easy_setopt(curl, CURLOPT_PROXY, "10.99.60.201:8080");// 代理
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sl_headers_);// 改协议头
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");


		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DefaultApiDataCallback);

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);


		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, DefaultApiHeaderCallback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

		res = curl_easy_perform(curl);   // 执行


		if (res != 0) {
			s_ret_ = curl_easy_strerror(res);
			QLOG_ERR(L"call api {0} error {1} {2}") << path_ << res << curl_easy_strerror(res);;

		}

		ApiResult::Parse(s_response_, api_result_);
		if (api_result_.GetCode() == 404)
		{
			QLOG_ERR(L"get url {0} sign error") << url;
		}
		if (!api_result_.Success())
		{
			s_ret_ = api_result_.GetMsg();
			if (!api_result_.ParseSuccess())
			{
				//string s = "服务器未响应";
				//s.append(s_response_).append("\n").append(url);
				QLOG_ERR(L"服务器未响应: {0}") << url;
				/*auto cb = [=](MsgBoxRet ret)
				{

				};
				ShowMsgBox(NULL, nbase::UTF8ToUTF16(s), cb);*/
			}
		}
	}
	else
	{
		s_ret_ = "网络初始化失败！";
	}
	return s_ret_;
}

//std::string  ApiCaller::CallGet(std::string &path_)
//{
//	
//	return CallGet(path_, std::map<std::string, std::string>());
//	/*std::string md5_s;
//	CalMd5(std::string("jyxb"), md5_s);
//
//	printf("%s\n", md5_s.c_str());*/
//}

std::string ApiCaller::generate_post_field_(std::map< std::string, std::string> &param_map_)
{
	std::string post_f_("");
	if (!param_map_.empty())
	{

		std::map< std::string, std::string>::iterator iter_ = param_map_.begin();

		while (iter_ != param_map_.end())
		{
			if (iter_ != param_map_.begin())
			{
				post_f_.append("&");
			}
			post_f_.append(iter_->first).append("=").append(iter_->second);
			iter_++;
		}
	}
	return post_f_;
}

std::string  ApiCaller::CallPost(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_, bool need_url_encode)
{
	for (auto i = param_map_.begin(); need_url_encode && i != param_map_.end(); i++)
	{
		string& v = i->second;
		v = urlencode(v);
	}

	CURLcode res;
	struct curl_slist *headers = sl_headers_;

	std::string s_ret_("");

	if (curl)
	{
		std::string post_f_ = generate_post_field_(param_map_);

		SS_MAP field_map_;
		field_map_["_post_field_"] = post_f_;


		std::string url = assemble_post_url_(path_, field_map_);
		std::string ua = build_user_agent_();
		std::string cooke_ = builde_cookie_();

		headers = curl_slist_append(headers, "Accept: */*");
		//headers = curl_slist_append(headers, "Transfer-Encoding: chunked");
		headers = curl_slist_append(headers, cooke_.c_str());
		headers = curl_slist_append(headers, "Expect:");
		string ts_str = "xy-nonce: " + field_map_["ts"];
		headers = curl_slist_append(headers, ts_str.c_str());
		string sign_str = "xy-sign: " + field_map_["sign"];
		headers = curl_slist_append(headers, sign_str.c_str());
		headers = curl_slist_append(headers, "xy-access-key: jyxb");

		if (strncmp(url.c_str(), "http://", 7) == 0)
		{
			// 方法1, 设定为不验证证书和HOST  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		}
		else if (strncmp(url.c_str(), "https://", 8) == 0)
		{
			// 方法2, 设定一个SSL判别证书, 未测试  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
			curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //设置一个证书文件  
		}

		//curl_easy_setopt(curl, CURLOPT_PROXY, "10.99.60.201:8080");// 代理
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);// 改协议头
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

		curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
		//curl_easy_setopt(curl, CURLOPT_HEADER, 1);

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_f_.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_f_.length());

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DefaultApiDataCallback);

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

		//curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, DefaultApiHeaderCallback);
		//curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
		//curl_easy_setopt(curl, CURLOPT_SSLVERSION, 3);
		res = curl_easy_perform(curl);   // 执行
		QLOG_APP(L"call post {0}") << url;
		if (res != 0) {
			s_ret_ = curl_easy_strerror(res);
			QLOG_ERR(L"call api {0} error {1} {2}") << path_ << res << s_ret_;

		}

		ApiResult::Parse(s_response_, api_result_);
		if (api_result_.GetCode() == 404)
		{
			ApiCommTool::GetInstance()->ApiSubmitLog(string("post sign error, body ").append(post_f_).append(" device=").append(GetMac()));
			QLOG_ERR(L"post url {0} sign error, body {1}") << url << post_f_;
			printf("=======login post body %s\n", post_f_.c_str());
		}
		if (!api_result_.Success())
		{
			s_ret_ = api_result_.GetMsg();
			if (!api_result_.ParseSuccess())
			{
				//string s = "服务器未响应";
				//s.append(s_response_).append("\n").append(url);
				QLOG_ERR(L"服务器未响应: {0}") << url;
				/*auto cb = [=](MsgBoxRet ret)
				{

				};
				ShowMsgBox(NULL, nbase::UTF8ToUTF16(s), cb);*/
			}
		}

	}
	else
	{
		s_ret_ = "网络初始化失败！";
	}
	return s_ret_;
}

size_t put_read_callback(char *ptr, size_t size, size_t nmemb, void *stream)
{
	//char* data = (char*)stream;
	return 0;
}

std::string ApiCaller::CallPut(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_)
{

	CURLcode res;

	std::string s_ret_("");

	if (curl)
	{
		std::string url = assemble_general_url_(path_, param_map_);
		std::string ua = build_user_agent_();

		QLOG_APP(L"call api {0}") << url;

		sl_headers_ = curl_slist_append(sl_headers_, "Accept: */*");
		sl_headers_ = curl_slist_append(sl_headers_, "Content-Type: text/plain");
		sl_headers_ = curl_slist_append(sl_headers_, builde_cookie_().c_str());
		string ts_str = "xy-nonce: " + param_map_["ts"];
		sl_headers_ = curl_slist_append(sl_headers_, ts_str.c_str());
		string sign_str = "xy-sign: " + param_map_["sign"];
		sl_headers_ = curl_slist_append(sl_headers_, sign_str.c_str());
		sl_headers_ = curl_slist_append(sl_headers_, "xy-access-key: jyxb");

		if (strncmp(url.c_str(), "http://", 7) == 0)
		{
			// 方法1, 设定为不验证证书和HOST  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		}
		else if (strncmp(url.c_str(), "https://", 8) == 0)
		{
			// 方法2, 设定一个SSL判别证书, 未测试  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
			curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //设置一个证书文件  
		}

		//curl_easy_setopt(curl, CURLOPT_PROXY, "10.99.60.201:8080");// 代理
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sl_headers_);// 改协议头
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
		curl_easy_setopt(curl, CURLOPT_PROXY, "");

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		//curl_easy_setopt(curl, CURLOPT_PUT, 1L);

		/*curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, 0);*/

		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, put_read_callback);
		curl_easy_setopt(curl, CURLOPT_READDATA, "");
		curl_easy_setopt(curl, CURLOPT_INFILESIZE, 0);	//size为0时不会调用read callback

		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");


		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DefaultApiDataCallback);

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);


		//curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, DefaultApiHeaderCallback);
		//curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

		res = curl_easy_perform(curl);   // 执行


		if (res != 0) {
			s_ret_ = curl_easy_strerror(res);
			QLOG_ERR(L"call api {0} error {1} {2}") << path_ << res << curl_easy_strerror(res);;

		}

		ApiResult::Parse(s_response_, api_result_);
		//api_result_ = ApiResult(s_response_);
		if (!api_result_.Success())
		{
			s_ret_ = api_result_.GetMsg();
		}
	}
	else
	{
		s_ret_ = "网络初始化失败！";
	}
	return s_ret_;
}

std::string ApiCaller::CallPostMultipartForm(std::string &path_, std::string &body_s, std::list<std::string>& header_list_, ApiResult& api_result_)
{

	CURLcode res;

	std::string s_ret_("");

	if (curl)
	{
		SS_MAP param_map_;
		param_map_["_body__"] = body_s;
		std::string url = assemble_post_url_(path_, param_map_);
		std::string ua = build_user_agent_();

		QLOG_APP(L"call api {0}") << url;

		sl_headers_ = curl_slist_append(sl_headers_, "Accept: */*");
		sl_headers_ = curl_slist_append(sl_headers_, "Expect:");
		sl_headers_ = curl_slist_append(sl_headers_, builde_cookie_().c_str());
		sl_headers_ = curl_slist_append(sl_headers_, "Content-Type: multipart/form-data");
		string ts_str = "xy-nonce: " + param_map_["ts"];
		sl_headers_ = curl_slist_append(sl_headers_, ts_str.c_str());
		string sign_str = "xy-sign: " + param_map_["sign"];
		sl_headers_ = curl_slist_append(sl_headers_, sign_str.c_str());
		sl_headers_ = curl_slist_append(sl_headers_, "xy-access-key: jyxb");

		auto iter = header_list_.begin();
		while (iter != header_list_.end())
		{
			sl_headers_ = curl_slist_append(sl_headers_, (*iter).c_str());
			iter++;
		}

		struct curl_httppost *post = NULL;
		struct curl_httppost *last = NULL;

		//	curl_formadd(&post, &last, CURLFORM_COPYNAME, "base64_data",
		//		CURLFORM_PTRCONTENTS, body_s.c_str(), CURLFORM_END);
		//curl_formadd(&post, &last, CURLFORM_PTRCONTENTS, body_s.c_str(), CURLFORM_BUFFERLENGTH, body_s.size(), CURLFORM_END);

		if (strncmp(url.c_str(), "http://", 7) == 0)
		{
			// 方法1, 设定为不验证证书和HOST  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		}
		else if (strncmp(url.c_str(), "https://", 8) == 0)
		{
			// 方法2, 设定一个SSL判别证书, 未测试  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
			curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //设置一个证书文件  
		}

		//curl_easy_setopt(curl, CURLOPT_PROXY, "10.99.60.201:8080");// 代理
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sl_headers_);// 改协议头
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		//y_setopt(curl, CURLOPT_HTTPPOST, post);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_s.c_str());
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body_s.length());
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");


		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DefaultApiDataCallback);

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);


		//curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, DefaultApiHeaderCallback);
		//curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

		res = curl_easy_perform(curl);   // 执行


		if (res != 0) {
			s_ret_ = curl_easy_strerror(res);
			QLOG_ERR(L"call api {0} error {1} {2}") << path_ << res << curl_easy_strerror(res);;

		}
		if (post)
		{
			curl_formfree(post);
		}

		ApiResult::Parse(s_response_, api_result_);
		if (!api_result_.Success())
		{
			s_ret_ = api_result_.GetMsg();
		}
	}
	else
	{
		s_ret_ = "网络初始化失败！";
	}
	return s_ret_;
}

std::string  ApiCaller::CallDelete(std::string &path_, std::map< std::string, std::string> &param_map_, ApiResult& api_result_)
{
	CURLcode res;

	struct curl_slist *headers = sl_headers_;

	std::string s_ret_("");

	if (curl)
	{
		std::string url = assemble_general_url_(path_, param_map_);
		std::string ua = build_user_agent_();

		QLOG_APP(L"call api {0}") << url;

		sl_headers_ = curl_slist_append(sl_headers_, "Accept: */*");
		sl_headers_ = curl_slist_append(sl_headers_, "Content-Type: text/plain");
		sl_headers_ = curl_slist_append(sl_headers_, builde_cookie_().c_str());
		string ts_str = "xy-nonce: " + param_map_["ts"];
		sl_headers_ = curl_slist_append(sl_headers_, ts_str.c_str());
		string sign_str = "xy-sign: " + param_map_["sign"];
		sl_headers_ = curl_slist_append(sl_headers_, sign_str.c_str());
		sl_headers_ = curl_slist_append(sl_headers_, "xy-access-key: jyxb");

		if (strncmp(url.c_str(), "http://", 7) == 0)
		{
			// 方法1, 设定为不验证证书和HOST  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		}
		else if (strncmp(url.c_str(), "https://", 8) == 0)
		{
			// 方法2, 设定一个SSL判别证书, 未测试  
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
			curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_CAINFO, "all.pem");    //设置一个证书文件  
		}

		//curl_easy_setopt(curl, CURLOPT_PROXY, "10.99.60.201:8080");// 代理
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, sl_headers_);// 改协议头
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, ua.c_str());
		curl_easy_setopt(curl, CURLOPT_PROXY, "");

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");

		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DefaultApiDataCallback);

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, DefaultApiHeaderCallback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);

		res = curl_easy_perform(curl);   // 执行
		QLOG_APP(L"call post {0}") << url;
		if (res != 0) {
			s_ret_ = curl_easy_strerror(res);
			QLOG_ERR(L"call api {0} error {1} {2}") << path_ << res << s_ret_;

		}

		ApiResult::Parse(s_response_, api_result_);
		if (api_result_.GetCode() == 404)
		{
			QLOG_ERR(L"delete url {0} sign error") << url;

		}

	}
	else
	{
		s_ret_ = "网络初始化失败！";
	}
	return s_ret_;
}

std::string ApiCaller::CalMd5ValidSign(std::map< std::string, std::string> &param_map_)
{
	vector<std::string> p_vector = vector<std::string>();

	p_vector.push_back(API_SIGN_KEY);
	auto iter_ = param_map_.begin();
	while (iter_ != param_map_.end())
	{
		p_vector.push_back(iter_->second);

		iter_++;
	}

	sort(p_vector.begin(), p_vector.end());

	std::string str_("");
	for (int i = 0; i < p_vector.size(); i++)
	{
		str_.append(p_vector[i]);
	}

	std::string cal_md5_;
	CalMd5(str_, cal_md5_);
	return cal_md5_;
}